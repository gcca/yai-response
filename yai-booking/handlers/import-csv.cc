#include <libpq-fe.h>

#include "../yai-booking-handlers.hpp"

namespace yai::booking::handlers {

Awaitable<void> ImportCSV(Stream &stream) {
  PGconn *conn = PQconnectdb("dbname=yai user=postgres");

  if (PQstatus(conn) != CONNECTION_OK) {
    PQfinish(conn);

    Messager messager = Messager::MakeErrors("Connection error");

    co_await stream.Write(messager.Flush(), messager.size());
    co_return;
  }

  PQfinish(conn);
}

} // namespace yai::booking::handlers
