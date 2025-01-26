#include <boost/json.hpp>
#include <libpq-fe.h>
#include <pyAi.hpp>
#include <sstream>
#include <xai.hpp>

static pyAi::ABISettings abi_settings;

namespace ImportJSON_Utils {

inline static nullptr_t ArrayParseError(const std::exception &e) {
  std::ostringstream oss;
  oss << "Error parsing JSON: " << e.what();

  PyErr_SetString(PyExc_ValueError, oss.str().c_str());
  return nullptr;
}

inline static bool ValidateName(const std::string &name) {
  return std::string_view::npos !=
         name.find_first_not_of("abcdefghijklmnopqrstuvwxyz"
                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "áéíóúñ ");
}

inline static bool ValidateVisitedAt(const std::string &visited_at) {
  return std::string_view::npos !=
         visited_at.find_first_not_of("0123456789-:TZ ");
}

inline static std::size_t
GetMapCount(std::unordered_map<std::string, std::size_t> &map,
            const std::string &key, std::size_t &count) {
  if (map.contains(key)) {
    return map[key];
  }

  map[key] = count;
  return count++;
}

inline static std::string CleanComment(const std::string_view &comment_raw) {
  static constexpr std::size_t OFFSET = 32;

  std::string comment;
  comment.reserve(comment_raw.size() + OFFSET);

  for (const char &c : comment_raw) {
    if (c == '\'') {
      comment.push_back('\'');
    }
    comment.push_back(c);
  }

  return comment;
}

inline static bool InsertConsultants(
    PGconn *conn,
    const std::unordered_map<std::string, std::size_t> &consultant_map) {
  std::ostringstream oss;
  oss << "INSERT INTO yai_booking_consultant (name) VALUES ";

  std::vector<const std::string *> order;
  order.resize(consultant_map.size());

  for (const auto &[name, id] : consultant_map) {
    order[id] = &name;
  }

  auto it = order.begin();
  oss << "('" << *it << "')";
  ++it;

  while (it != order.end()) {
    oss << ", ('" << *it << "')";
    ++it;
  }

  oss << " RETURNING id";

  PGresult *res = PQexec(conn, oss.str().c_str());

  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    PyErr_SetString(PyExc_RuntimeError, PQresultErrorMessage(res));
    PQclear(res);
    PQfinish(conn);
    return true;
  }

  PQclear(res);

  return false;
}

} // namespace ImportJSON_Utils

static PyObject *ImportJSON(PyObject *, PyObject *bytes) {
  if (!PyBytes_Check(bytes)) {
    PyErr_SetString(PyExc_TypeError, "Expected a bytes");
    return nullptr;
  }

  Py_ssize_t size = PyBytes_Size(bytes);
  if (size < 0) {
    PyErr_SetString(PyExc_TypeError, "Empty bytes");
    return nullptr;
  }

  const char *buffer = PyBytes_AsString(bytes);
  if (!buffer) {
    PyErr_SetString(PyExc_TypeError, "Error getting buffer");
    return nullptr;
  }

  boost::json::array payload_array;
  try {
    payload_array = boost::json::parse(buffer).as_array();
  } catch (const std::exception &e) {
    return ImportJSON_Utils::ArrayParseError(e);
  }

  std::vector<std::pair<boost::json::object, std::vector<const char *>>>
      bad_objects;
  bad_objects.reserve(payload_array.size());

  std::vector<std::tuple<std::size_t, std::size_t, std::string, std::string>>
      bulk_objects;
  bulk_objects.reserve(payload_array.size());

  static const char *PARSE_MESSAGE = "Error parsing object",
                    *MISSING_CONSULTANT = "Missing consultant member",
                    *MISSING_CUSTOMER = "Missing customer member",
                    *MISSING_VISITED_AT = "Missing visited_at member",
                    *MISSING_COMMENT = "Missing comment member",
                    *INVALID_CONSULTANT = "Invalid consultant name",
                    *INVALID_CUSTOMER = "Invalid customer name",
                    *INVALID_VISITED_AT = "Invalid visited_at format";

  std::unordered_map<std::string, std::size_t> consultant_map, customer_map;
  std::size_t consultant_count = 0, customer_count = 0;

  for (const auto &item : payload_array) {
    std::vector<const char *> errors;
    errors.reserve(3);

    boost::json::object object;
    try {
      object = item.as_object();
    } catch (const std::exception &) {
      errors.emplace_back(PARSE_MESSAGE);
      bad_objects.emplace_back(std::move(object), errors);
      continue;
    }

    std::string consultant;
    try {
      consultant = std::move(object["consultant"].as_string());
    } catch (const std::exception &) {
      errors.emplace_back(MISSING_CONSULTANT);
    }

    std::string customer;
    try {
      customer = std::move(object["customer"].as_string());
    } catch (const std::exception &) {
      errors.emplace_back(MISSING_CUSTOMER);
    }

    std::string visited_at;
    try {
      visited_at = std::move(object["visited_at"].as_string());
    } catch (const std::exception &) {
      errors.emplace_back(MISSING_VISITED_AT);
    }

    std::string_view comment_raw;
    try {
      comment_raw = object["comment"].as_string();
    } catch (const std::exception &) {
      errors.emplace_back(MISSING_COMMENT);
    }

    if (ImportJSON_Utils::ValidateName(consultant)) {
      errors.emplace_back(INVALID_CONSULTANT);
    }

    if (ImportJSON_Utils::ValidateName(customer)) {
      errors.emplace_back(INVALID_CUSTOMER);
    }

    if (ImportJSON_Utils::ValidateVisitedAt(visited_at)) {
      errors.emplace_back(INVALID_VISITED_AT);
    }

    if (!errors.empty()) {
      bad_objects.emplace_back(std::move(object), std::move(errors));
      continue;
    }

    std::size_t consultant_value = ImportJSON_Utils::GetMapCount(
        consultant_map, consultant, consultant_count);

    std::size_t customer_value =
        ImportJSON_Utils::GetMapCount(customer_map, customer, customer_count);

    const std::string comment = ImportJSON_Utils::CleanComment(comment_raw);

    bulk_objects.emplace_back(consultant_value, customer_value,
                              std::move(visited_at), std::move(comment));
  }

  if (bulk_objects.empty()) {
    PyErr_SetString(PyExc_ValueError, "No valid objects found");
    return nullptr;
  }

  PGconn *conn = PQconnectdb(abi_settings.conninfo);

  if (PQstatus(conn) != CONNECTION_OK) {
    PyErr_SetString(PyExc_ConnectionError, PQerrorMessage(conn));
    PQfinish(conn);
    return nullptr;
  }

  if (ImportJSON_Utils::InsertConsultants(conn, consultant_map)) {
    PyErr_SetString(PyExc_RuntimeError, "Error inserting consultants");
    return nullptr;
  }

  std::ostringstream oss;
  oss << "INSERT INTO booking (consultant, customer, visited_at, comment) "
         "VALUES ";

  auto it = bulk_objects.begin();

  {
    const auto &[consultant, customer, visited_at, comment] = *it;
    oss << "('" << consultant << "', '" << customer << "', '" << visited_at
        << "', '" << comment << "')";
    ++it;
  }

  while (it != bulk_objects.end()) {
    const auto &[consultant, customer, visited_at, comment] = *it;

    oss << ", ('" << consultant << "', '" << customer << "', '" << visited_at
        << "', '" << comment << "')";
    ++it;
  }

  PGresult *res = PQexec(conn, oss.str().c_str());

  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    PyErr_SetString(PyExc_RuntimeError, PQresultErrorMessage(res));
    PQclear(res);
    PQfinish(conn);
    Py_RETURN_NONE;
  }

  PQclear(res);
  PQfinish(conn);

  Py_RETURN_NONE;
}

inline static void OSSAppendNames(std::ostringstream &oss,
                                  const std::vector<std::string_view> &names) {
  auto it = names.begin();
  oss << "'" << *it << "'";
  ++it;

  while (it != names.end()) {
    oss << ", '" << *it << "'";
    ++it;
  }
}

static PyObject *ImportCSVConsultants(PyObject *, PyObject *bytes) {
  if (!PyBytes_Check(bytes)) {
    PyErr_SetString(PyExc_TypeError, "Expected a bytes");
    return nullptr;
  }

  Py_ssize_t buffer_ssize = PyBytes_Size(bytes);
  if (buffer_ssize < 0) {
    PyErr_SetString(PyExc_TypeError, "Empty bytes");
    return nullptr;
  }
  std::size_t buffer_size = static_cast<std::size_t>(buffer_ssize);

  const char *buffer = PyBytes_AsString(bytes);
  if (!buffer) {
    PyErr_SetString(PyExc_TypeError, "Error getting buffer");
    return nullptr;
  }

  std::vector<std::string_view> names;
  names.reserve(buffer_size / 10);

  std::size_t start = 0;
  for (std::size_t i = 0; i < buffer_size; ++i) {
    if (buffer[i] == '\n' || buffer[i] == '\0') {
      names.emplace_back(buffer + start, i - start);
      start = i + 1;
    }
  }

  std::ostringstream oss;
  oss << "INSERT INTO yai_booking_consultant (name) SELECT unnest(ARRAY[ ";
  OSSAppendNames(oss, names);
  oss << "]) WHERE NOT EXISTS (SELECT 1 FROM yai_booking_consultant WHERE "
         "name = ANY(ARRAY[ ";
  OSSAppendNames(oss, names);
  oss << "]))";

  PGconn *conn = PQconnectdb(abi_settings.conninfo);
  if (PQstatus(conn) != CONNECTION_OK) {
    PyErr_SetString(PyExc_ConnectionError, PQerrorMessage(conn));
    PQfinish(conn);
    return nullptr;
  }

  PGresult *res = PQexec(conn, oss.str().c_str());

  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    PyErr_SetString(PyExc_RuntimeError, PQresultErrorMessage(res));
    PQclear(res);
    PQfinish(conn);
    return nullptr;
  }

  PQclear(res);
  PQfinish(conn);

  Py_RETURN_NONE;
}

static PyObject *ImportCSVCustomers(PyObject *, PyObject *bytes) {
  if (!PyBytes_Check(bytes)) {
    PyErr_SetString(PyExc_TypeError, "Expected a bytes");
    return nullptr;
  }

  Py_ssize_t buffer_ssize = PyBytes_Size(bytes);
  if (buffer_ssize < 0) {
    PyErr_SetString(PyExc_TypeError, "Empty bytes");
    return nullptr;
  }
  std::size_t buffer_size = static_cast<std::size_t>(buffer_ssize);

  const char *buffer = PyBytes_AsString(bytes);
  if (!buffer) {
    PyErr_SetString(PyExc_TypeError, "Error getting buffer");
    return nullptr;
  }

  std::vector<std::string_view> names;
  names.reserve(buffer_size / 10);

  std::size_t start = 0;
  for (std::size_t i = 0; i < buffer_size; ++i) {
    if (buffer[i] == '\n' || buffer[i] == '\0') {
      names.emplace_back(buffer + start, i - start);
      start = i + 1;
    }
  }

  std::ostringstream oss;
  oss << "INSERT INTO yai_booking_customer (name) SELECT unnest(ARRAY[ ";
  OSSAppendNames(oss, names);
  oss << "]) WHERE NOT EXISTS (SELECT 1 FROM yai_booking_customer WHERE "
         "name = ANY(ARRAY[ ";
  OSSAppendNames(oss, names);
  oss << "]))";

  PGconn *conn = PQconnectdb(abi_settings.conninfo);
  if (PQstatus(conn) != CONNECTION_OK) {
    PyErr_SetString(PyExc_ConnectionError, PQerrorMessage(conn));
    PQfinish(conn);
    return nullptr;
  }

  PGresult *res = PQexec(conn, oss.str().c_str());

  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    PyErr_SetString(PyExc_RuntimeError, PQresultErrorMessage(res));
    PQclear(res);
    PQfinish(conn);
    return nullptr;
  }

  PQclear(res);
  PQfinish(conn);

  Py_RETURN_NONE;
}

namespace ImportCSVBooking_Utils {

inline static bool LoadConsultants(
    PGconn *conn,
    std::unordered_map<std::string_view, std::string_view> &consultant_map,
    PGresult *&pgres) {
  consultant_map.reserve(128);

  pgres = PQexec(conn, "SELECT id, name FROM yai_booking_consultant LIMIT 128");

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    PyErr_SetString(PyExc_RuntimeError, PQresultErrorMessage(pgres));
    return true;
  }

  const int rows = PQntuples(pgres);

  for (int i = 0; i < rows; ++i)
    consultant_map.emplace(PQgetvalue(pgres, i, 1), PQgetvalue(pgres, i, 0));

  return false;
}

inline static bool LoadCustomers(
    PGconn *conn,
    std::unordered_map<std::string_view, std::string_view> &customer_map,
    PGresult *&pgres) {
  customer_map.reserve(256);

  pgres = PQexec(conn, "SELECT id, name FROM yai_booking_customer LIMIT 256");

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    PyErr_SetString(PyExc_RuntimeError, PQresultErrorMessage(pgres));
    return true;
  }

  const int rows = PQntuples(pgres);

  for (int i = 0; i < rows; ++i)
    customer_map.emplace(PQgetvalue(pgres, i, 1), PQgetvalue(pgres, i, 0));

  return false;
}

} // namespace ImportCSVBooking_Utils

static PyObject *ImportCSVBooking(PyObject *, PyObject *bytes) {
  if (!PyBytes_Check(bytes)) {
    PyErr_SetString(PyExc_TypeError, "Expected a bytes");
    return nullptr;
  }

  Py_ssize_t buffer_ssize = PyBytes_Size(bytes);
  if (buffer_ssize < 0) {
    PyErr_SetString(PyExc_TypeError, "Empty bytes");
    return nullptr;
  }

  const char *buffer = PyBytes_AsString(bytes);
  if (!buffer) {
    PyErr_SetString(PyExc_TypeError, "Error getting buffer");
    return nullptr;
  }

  PGconn *conn = PQconnectdb(abi_settings.conninfo);

  if (PQstatus(conn) != CONNECTION_OK) {
    PyErr_SetString(PyExc_ConnectionError, PQerrorMessage(conn));
    PQfinish(conn);
    return nullptr;
  }

  std::unordered_map<std::string_view, std::string_view> consultant_map,
      customer_map;
  PGresult *consultant_pgres = nullptr, *customer_pgres = nullptr;

  if (ImportCSVBooking_Utils::LoadConsultants(conn, consultant_map,
                                              consultant_pgres)) {
    PQclear(consultant_pgres);
    PQfinish(conn);
    return nullptr;
  }

  if (ImportCSVBooking_Utils::LoadCustomers(conn, customer_map,
                                            customer_pgres)) {
    PQclear(consultant_pgres);
    PQclear(customer_pgres);
    PQfinish(conn);
    return nullptr;
  }

  std::vector<std::tuple<std::string_view, std::string_view, std::string_view,
                         std::string_view>>
      bulk_lines;
  bulk_lines.reserve(512);

  std::string_view buffer_view(buffer, static_cast<std::size_t>(buffer_ssize));
  std::size_t pos = buffer_view.find('\n') + 1;

  while (pos < buffer_view.size()) {
    std::size_t next = buffer_view.find(',', pos);
    if (next == std::string_view::npos)
      break;
    std::string_view consultant = buffer_view.substr(pos, next - pos);
    pos = next + 1;

    next = buffer_view.find(',', pos);
    if (next == std::string_view::npos)
      break;
    std::string_view customer = buffer_view.substr(pos, next - pos);
    pos = next + 1;

    next = buffer_view.find(',', pos);
    if (next == std::string_view::npos)
      break;
    std::string_view visited_at = buffer_view.substr(pos, next - pos);
    pos = next + 1;

    next = buffer_view.find('\n', pos);
    if (next == std::string_view::npos)
      break;
    std::string_view comment = buffer_view.substr(pos + 1, next - pos - 2);
    pos = next + 1;

    std::string_view::size_type pos_quote = comment.find('\'');
    while (pos_quote != std::string_view::npos) {
      const_cast<char *>(comment.data())[pos_quote] = '"';
      pos_quote = comment.find('\'', pos_quote + 1);
    }

    bulk_lines.emplace_back(consultant, customer, visited_at, comment);
  }

  if (bulk_lines.empty()) {
    PyErr_SetString(PyExc_ValueError, "No valid lines found");
    return nullptr;
  }

  std::ostringstream oss;
  oss << "INSERT INTO yai_booking_book (consultant_id, customer_id, "
         "visited_at, comment) VALUES ";

  auto it = bulk_lines.begin();

  {
    const auto &[consultant, customer, visited_at, comment] = *it;
    oss << "(" << consultant_map[consultant] << ", " << customer_map[customer]
        << ", '" << visited_at << "', '" << comment << "')";
    ++it;
  }

  while (it != bulk_lines.end()) {
    const auto &[consultant, customer, visited_at, comment] = *it;
    oss << ", (" << consultant_map[consultant] << ", " << customer_map[customer]
        << ", '" << visited_at << "', '" << comment << "')";
    ++it;
  }

  PGresult *booking_pgres = PQexec(conn, oss.str().c_str());

  if (PQresultStatus(booking_pgres) != PGRES_COMMAND_OK) {
    PyErr_SetString(PyExc_RuntimeError, PQresultErrorMessage(booking_pgres));
    PQclear(booking_pgres);
    PQclear(consultant_pgres);
    PQclear(customer_pgres);
    PQfinish(conn);
    return nullptr;
  }

  PQclear(booking_pgres);
  PQclear(consultant_pgres);
  PQclear(customer_pgres);
  PQfinish(conn);

  Py_RETURN_NONE;
}

static PyObject *AiConsultantsSummary(PyObject *) {
  PGconn *conn = PQconnectdb(abi_settings.conninfo);

  if (PQstatus(conn) != CONNECTION_OK) {
    PyErr_SetString(PyExc_ConnectionError, PQerrorMessage(conn));
    PQfinish(conn);
    return nullptr;
  }

  PGresult *pgres_message =
      PQexec(conn, "SELECT message FROM yai_booking_message LIMIT 1");

  if (PQresultStatus(pgres_message) != PGRES_TUPLES_OK) {
    PyErr_SetString(PyExc_RuntimeError, PQresultErrorMessage(pgres_message));
    PQclear(pgres_message);
    PQfinish(conn);
    return nullptr;
  }

  const char *message = PQgetvalue(pgres_message, 0, 0);

  PGresult *pgres_books =
      PQexec(conn, "SELECT C.name, D.name, B.comment FROM yai_booking_book B "
                   "JOIN yai_booking_consultant C ON B.consultant_id = C.id "
                   "JOIN yai_booking_customer D ON B.customer_id = D.id");

  if (PQresultStatus(pgres_books) != PGRES_TUPLES_OK) {
    PyErr_SetString(PyExc_RuntimeError, PQresultErrorMessage(pgres_books));
    PQclear(pgres_books);
    PQclear(pgres_message);
    PQfinish(conn);
    return nullptr;
  }

  const int rows = PQntuples(pgres_books);
  std::ostringstream oss;

  oss << message << "<elements>\n";
  for (int i = 0; i < rows; ++i) {
    oss << "\t<item>\n\t\t<consultant>" << PQgetvalue(pgres_books, i, 0)
        << "</consultant>\n\t\t<customer>" << PQgetvalue(pgres_books, i, 1)
        << "</customer>\n\t\t<comment>" << PQgetvalue(pgres_books, i, 2)
        << "</comment>\n\t</item>\n";
  }
  oss << "</elements>\n";

  PQclear(pgres_books);
  PQclear(pgres_message);
  PQfinish(conn);

  std::unique_ptr<xai::Client> client =
      xai::Client::Make(abi_settings.xai_api_key);

  if (!client) {
    PyErr_SetString(PyExc_RuntimeError, "Error creating client");
    return nullptr;
  }

  std::unique_ptr<xai::Messages> messages =
      xai::Messages::Make(abi_settings.xai_model);
  messages->AddU(oss.str());

  std::unique_ptr<xai::Choices> choices = client->ChatCompletion(messages);

  if (!choices) {
    PyErr_SetString(PyExc_RuntimeError, "Error getting choices");
    return nullptr;
  }

  std::string_view res = choices->first();

  PyObject *pyres = PyBytes_FromStringAndSize(
      res.data(), static_cast<Py_ssize_t>(res.size()));

  if (!pyres) {
    PyErr_SetString(PyExc_RuntimeError, "Error creating bytes");
    return nullptr;
  }

  return pyres;
}

static PyMethodDef m_methods[] = {
    {"ImportJSON", ImportJSON, METH_O, "Import JSON"},
    {"ImportCSVConsultants", ImportCSVConsultants, METH_O,
     "Import Consultants from CSV"},
    {"ImportCSVCustomers", ImportCSVCustomers, METH_O,
     "Import Customers from CSV"},
    {"ImportCSVBooking", ImportCSVBooking, METH_O, "Import Booking from CSV"},
    {"AiConsultantsSummary", _PyCFunction_CAST(AiConsultantsSummary),
     METH_NOARGS, "Consultants Summary"},
    {nullptr, nullptr, 0, nullptr}};

static struct PyModuleDef pymoduledef = {PyModuleDef_HEAD_INIT,
                                         "yai_booking_abi",
                                         "yAI Booking ABI Module",
                                         -1,
                                         m_methods,
                                         nullptr,
                                         nullptr,
                                         nullptr,
                                         nullptr};

PyMODINIT_FUNC PyInit_yai_booking_abi(void) {
  if (pyAi::InitSettings(abi_settings))
    return nullptr;
  return PyModule_Create(&pymoduledef);
}
