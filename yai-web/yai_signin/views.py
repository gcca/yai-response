from django.contrib.auth import authenticate, login
from django.http import HttpResponseRedirect
from django.urls import reverse_lazy
from django.views.generic import FormView

from .forms import IndexForm


class IndexView(FormView):

    template_name = "yai/signin/index.html"
    form_class = IndexForm
    success_url = reverse_lazy("yai_chat:index")

    def form_valid(self, form: IndexForm) -> HttpResponseRedirect:
        cleaned_data = form.cleaned_data

        username = cleaned_data.get("username")
        password = cleaned_data.get("password")

        user = authenticate(username=username, password=password)

        if user is None:
            form.add_error(None, "Invalid username or password")
            return self.form_invalid(form)

        login(self.request, user)
        return super().form_valid(form)
