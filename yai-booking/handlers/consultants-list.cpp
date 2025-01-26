#include <cstdint>
#include <libpq-fe.h>

#include "../yai-booking-handlers.hpp"

namespace yai::booking::handlers {

Awaitable<void> ListConsultants(Stream &stream) {
  PGconn *conn = PQconnectdb("dbname=yai user=postgres");

  if (PQstatus(conn) != CONNECTION_OK) {
    PQfinish(conn);

    Messager messager = Messager::MakeErrors("Connection error");

    co_await stream.Write(messager.Flush(), messager.size());
    co_return;
  }

  PGresult *res = PQexec(conn, "SELECT id, name FROM yai_booking_consultant");

  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    PQclear(res);
    PQfinish(conn);

    Messager messager = Messager::MakeErrors("Execution error");

    co_await stream.Write(messager.Flush(), messager.size());
    co_return;
  }

  int n = PQntuples(res);

  Messager messager{static_cast<std::size_t>(n) * 128};
  messager.Status(0);

  messager.AppendNarrow(n);

  for (int i = 0; i < n; i++) {
    int id = std::stoi(PQgetvalue(res, i, 0));
    messager.AppendNarrow(id);

    const char *name = PQgetvalue(res, i, 1);

    messager.AppendNarrow(static_cast<std::uint32_t>(std::strlen(name)));
    messager.AppendNarrow(name);
  }

  co_await stream.Write(messager.Flush(), messager.size());
}

} // namespace yai::booking::handlers
