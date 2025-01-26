import socket
import struct
from abc import ABC, abstractmethod
from typing import override

import yai_booking_abi
from django.core.files.uploadedfile import InMemoryUploadedFile
from django.http import (
    HttpRequest,
    HttpResponse,
    HttpResponseRedirect,
    JsonResponse,
)
from django.middleware.csrf import get_token
from django.urls import reverse_lazy
from django.views.generic import FormView, View

from .forms import ImportFileForm


class Message:
    def __init__(self, mi):
        self.mi = mi

    @classmethod
    def from_bytes(cls, data):
        mi = struct.unpack("Q", data)
        return cls(mi)

    def to_bytes(self):
        return struct.pack("Q", self.mi)


def index(_):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(("localhost", 12345))

    try:
        message = Message(0)
        sock.sendall(message.to_bytes())

        data = sock.recv(8)

        s, r = struct.unpack("II", data)

        if s:
            data = sock.recv(r)
            n = struct.unpack_from("I", data)[0]
            offset = 4
            result = []

            while n > 0:
                size = struct.unpack_from("I", data, offset)[0]
                result.append(
                    struct.unpack_from(f"{size}s", data, offset + 4)[
                        0
                    ].decode()
                )
                offset += size + 4
                n -= 1

            return JsonResponse({"errors": result}, status=400)

        data = sock.recv(r)

        n = struct.unpack_from("I", data)[0]

        result = []
        offset = 4

        while n > 0:
            value_id = struct.unpack_from("I", data, offset)[0]
            name_size = struct.unpack_from("I", data, offset + 4)[0]
            value_name = struct.unpack_from(f"{name_size}s", data, offset + 8)[
                0
            ].decode()

            result.append({"id": value_id, "name": value_name})

            offset += name_size + 8
            n -= 1

    finally:
        sock.close()

    return JsonResponse({"result": result})


class CTokenView(View):

    def get(self, req: HttpRequest) -> HttpResponse:
        return JsonResponse({"result": get_token(req)})


class SimpleImportView(FormView, ABC):

    template_name = "booking/simple-import.html"
    form_class = ImportFileForm

    def form_valid(self, form: ImportFileForm) -> HttpResponseRedirect:
        data_file: InMemoryUploadedFile = form.cleaned_data["file"]
        self.Import(data_file.read())
        return super().form_valid(form)

    @abstractmethod
    def Import(self, data: bytes) -> None: ...


class ImportCSVConsultantsView(SimpleImportView):

    success_url = reverse_lazy("yai_booking:import:csv:consultants")

    @override
    def Import(self, data: bytes) -> None:
        yai_booking_abi.ImportCSVConsultants(data)


class ImportCSVCustomersView(SimpleImportView):

    success_url = reverse_lazy("yai_booking:import:csv:customers")

    @override
    def Import(self, data: bytes) -> None:
        yai_booking_abi.ImportCSVCustomers(data)


class ImportCSVBooking(SimpleImportView):

    success_url = reverse_lazy("yai_booking:import:csv:booking")

    @override
    def Import(self, data: bytes) -> None:
        yai_booking_abi.ImportCSVBooking(data)


class AiConsultantsSummaryView(View):

    def get(self, _: HttpRequest) -> HttpResponse:
        res = yai_booking_abi.AiConsultantsSummary()
        return HttpResponse(res, content_type="application/json")
