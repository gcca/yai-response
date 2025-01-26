from datetime import datetime
from queue import Queue
from threading import Thread
from typing import Any, Dict, Iterator, List, Optional, Tuple, cast

import yai_chat_abi
from django.contrib.auth.mixins import LoginRequiredMixin
from django.core.cache import cache
from django.http import (
    HttpRequest,
    HttpResponse,
    HttpResponseBase,
    StreamingHttpResponse,
)
from django.shortcuts import render
from django.template.loader import render_to_string
from django.views.generic import TemplateView, View
from htmlmin import minify
from markdown import markdown


class ChatViewSupport(LoginRequiredMixin, TemplateView):

    def get_context_data(self, **_: Any) -> Any:
        history_cache = HistoryCache(self.request)
        history = history_cache.Get()
        return {"history": Apply(history)}


class PartialChatView(ChatViewSupport):

    template_name = "yai/chat/partial.html"


class MessageChatView(ChatViewSupport):

    template_name = "yai/chat/message.html"


class MessageView(LoginRequiredMixin, View):

    def post(self, request: HttpRequest) -> HttpResponseBase:
        question: str = request.body.decode()

        if not question:
            return HttpResponse(b"")

        history_cache = HistoryCache(request)

        if question == "/c":
            history_cache.Delete()
            return HttpResponse(b"")

        history = history_cache.Get()

        try:
            yai_chat_abi.ProcessMessage(history, question, Scope(request))
        except Exception as error:
            history.append((question, f"Error: {error}"))

        history_cache.Put(history)

        return render(
            request,
            "yai/chat/item.html",
            context={"history": Apply(history)},
        )


class PartialView(LoginRequiredMixin, View):

    def get(self, request: HttpRequest) -> HttpResponseBase:
        arg_cache = ArgCache(request)

        question: Optional[str] = arg_cache.Get()

        if not question:
            return HttpResponse()

        arg_cache.Delete()

        queue = Queue[str]()

        history_cache = HistoryCache(request)
        history = history_cache.Get()

        def process() -> None:
            try:
                yai_chat_abi.ProcessPartial(
                    history, question, Scope(request), queue.put
                )
            except Exception as error:
                history.append((question, f"Error: {error}"))

            queue.put("data: [DONE]")

        thread = Thread(target=process)

        def content() -> Iterator[str]:
            thread.start()
            history.append((question, ""))

            try:
                while True:
                    part: str = queue.get()

                    if part == "data: [DONE]":
                        queue.task_done()
                        break

                    q, a = history[-1]
                    history[-1] = (q, a + part)

                    s = minify(
                        render_to_string(
                            "yai/chat/item.html",
                            {"history": Apply(history)},
                        ),
                        remove_empty_space=True,
                    )

                    queue.task_done()

                    yield f"data: {s}\n\n"

            except (BrokenPipeError, ConnectionResetError):
                pass

            finally:
                thread.join()
                history_cache.Put(history)

            yield "data: [DONE]\n\n"

        response = StreamingHttpResponse(
            content(), content_type="text/event-stream"
        )
        response["Cache-Control"] = "no-cache"
        return response

    def post(self, request: HttpRequest) -> HttpResponseBase:
        arg_cache = ArgCache(request)

        arg: Optional[bytes] = request.body
        if arg:
            arg_cache.Put(arg.decode())

        return HttpResponse()


def Apply(history: List[Tuple[str, str]]) -> List[Tuple[str, str]]:
    return [(q, markdown(a)) for q, a in history]


def Scope(request: HttpRequest) -> Dict[str, str]:
    return {
        "username": cast(Any, request).user.username,
        "today": datetime.now().strftime("%Y-%m-%d"),
    }


class HistoryCache:

    def __init__(self, request: HttpRequest) -> None:
        username = cast(Any, request).user.username
        self._key = f"{username}-history"

    def Get(self) -> List[Tuple[str, str]]:
        history: Optional[List[Tuple[str, str]]] = cache.get(self._key)

        if not history:
            history = []

        return history

    def Put(self, history: List[Tuple[str, str]]) -> None:
        cache.set(self._key, history)

    def Delete(self) -> None:
        cache.delete(self._key)


class ArgCache:

    def __init__(self, request: HttpRequest) -> None:
        username = cast(Any, request).user.username
        self._key = f"{username}-arg"

    def Get(self) -> Optional[str]:
        return cache.get(self._key)

    def Put(self, arg: str) -> None:
        cache.set(self._key, arg)

    def Delete(self) -> None:
        cache.delete(self._key)
