from django.urls import include, path

from .views import (
    AiConsultantsSummaryView,
    CTokenView,
    ImportCSVBooking,
    ImportCSVConsultantsView,
    ImportCSVCustomersView,
)

app_name = "yai_booking"

csv_import_urls = (
    (
        path(
            "consultants/",
            ImportCSVConsultantsView.as_view(),
            name="consultants",
        ),
        path("customers/", ImportCSVCustomersView.as_view(), name="customers"),
        path("booking/", ImportCSVBooking.as_view(), name="booking"),
    ),
    "csv",
)

import_urls = (
    (path("csv/", include(csv_import_urls)),),
    "import",
)

ai_urls = (
    (path("consultants-summary/", AiConsultantsSummaryView.as_view()),),
    "ai",
)

urlpatterns = (
    path("ctoken/", CTokenView.as_view()),
    path("import/", include(import_urls)),
    path("ai/", include(ai_urls)),
)
