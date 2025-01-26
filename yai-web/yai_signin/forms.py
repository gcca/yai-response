from django.forms import CharField, Form


class IndexForm(Form):
    username = CharField(max_length=64)
    password = CharField(max_length=128)
