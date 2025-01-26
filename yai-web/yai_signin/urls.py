from django.urls import path

from .views import IndexView

app_name = "yai_signin"

urlpatterns = (path("", IndexView.as_view(), name="index"),)
