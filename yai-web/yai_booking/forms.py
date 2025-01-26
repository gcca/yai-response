from django import forms


class ImportFileForm(forms.Form):
    file = forms.FileField()
