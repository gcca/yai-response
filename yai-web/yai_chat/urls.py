from django.urls import path

from .views import MessageChatView, MessageView, PartialChatView, PartialView

app_name = "yai_chat"

urlpatterns = (
    path("message/", MessageView.as_view(), name="message"),
    path("partial/", PartialView.as_view(), name="partial"),
    path("", PartialChatView.as_view(), name="index"),
    path("i/message/", MessageChatView.as_view(), name="message-chat"),
)
