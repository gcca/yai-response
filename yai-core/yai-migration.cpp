#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <libpq-fe.h>

#include "yai-migration.hpp"

namespace {

inline static const char *host = "localhost", *db = "yai", *user = "",
                         *pass = "", *port = "5432";

inline static int ExecQ(const std::string &q) {
  std::ostringstream oss;
  oss << "host=" << host << " dbname=" << db << " port=" << port;

  if (*user && std::strlen(user) > 0)
    oss << " user=" << user;

  if (*pass && std::strlen(pass) > 0)
    oss << " password=" << pass;

  PGconn *conn = PQconnectdb(oss.str().c_str());

  if (PQstatus(conn) != CONNECTION_OK) {
    std::cerr << "Connection to database failed: " << PQerrorMessage(conn)
              << std::endl;
    PQfinish(conn);
    return EXIT_FAILURE;
  }

  PGresult *res = PQexec(conn, q.c_str());
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
    std::cerr << "Error: " << PQresultErrorMessage(res) << std::endl;

  PQclear(res);
  PQfinish(conn);

  return EXIT_SUCCESS;
}

inline static std::string pigment(std::string sql) {
  const std::string endw = "\033[0m";
  const std::string quot = "\033[32m";
  const std::string kwd1 = "\033[34m";
  const std::string kwd2 = "\033[33m";
  const std::string keywords1[] = {"SELECT",
                                   "FROM",
                                   "WHERE",
                                   "AND",
                                   "OR",
                                   "INSERT",
                                   "UPDATE",
                                   "DELETE",
                                   "CREATE",
                                   "DROP",
                                   "TABLE",
                                   "INDEX",
                                   "JOIN",
                                   "LIMIT",
                                   "GROUP BY",
                                   "ORDER BY",
                                   "IF",
                                   "NOT",
                                   "EXISTS",
                                   "PRIMARY KEY",
                                   "NULL",
                                   "DEFAULT",
                                   "CURRENT_TIMESTAMP",
                                   "REFERENCES"};

  for (const auto &keyword : keywords1) {
    std::size_t pos = 0;
    while ((pos = sql.find(keyword, pos)) != std::string::npos) {
      sql.replace(pos, keyword.length(), kwd1 + keyword + endw);
      pos += kwd1.length() + keyword.length() + endw.length();
    }
  }

  const std::string keywords2[] = {"VARCHAR", "INT", "SERIAL", " TIMESTAMP"};

  for (const auto &keyword : keywords2) {
    std::size_t pos = 0;
    while ((pos = sql.find(keyword, pos)) != std::string::npos) {
      sql.replace(pos, keyword.length(), kwd2 + keyword + endw);
      pos += kwd2.length() + keyword.length() + endw.length();
    }
  }

  std::size_t start_pos = 0;
  while ((start_pos = sql.find('\'', start_pos)) != std::string::npos) {
    std::size_t end_pos = sql.find('\'', start_pos + 1);
    if (end_pos != std::string::npos) {
      sql.replace(start_pos, end_pos - start_pos + 1,
                  quot + sql.substr(start_pos, end_pos - start_pos + 1) + endw);
      start_pos = end_pos + quot.length() + endw.length();
    }
  }

  return sql;
}

inline static void Usage(const char *name) {
  std::cerr << "Usage: " << name << " <action>\n\nactions:\n\t" << std::left
            << std::setw(12) << "migrate" << "Create db schema\n\t"
            << std::setw(12) << "rollback" << "Remove db schema\n\t"
            << std::setw(12) << "fixtures" << "Load fixtures\n\t"
            << std::setw(12) << "drop" << "Drop db\n\t" << std::setw(12)
            << "show" << "Show queries\n\t" << std::setw(12) << "help"
            << "Show this message" << std::endl;
}

inline static int Run(const char *q, const char *label) {
  std::cout << "\033[37m" << label;
  if (!q or !*q or !std::strlen(q)) {
    std::cout << "\033[33m SKIPPED\033[0m (query not defined)" << std::endl;
    return EXIT_SUCCESS;
  }

  try {
    ExecQ(q);
    std::cout << "\033[32m DONE\033[0m" << std::endl;
  } catch (const std::exception &e) {
    std::cout << "FAILED" << std::endl;
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

} // namespace

namespace yai::migration {

Migration::Migration(const char *migrate_q, const char *rollback_q,
                     const char *fixtures_q, const char *drop_q)
    : migrate_q_{migrate_q}, rollback_q_{rollback_q}, fixtures_q_{fixtures_q},
      drop_q_{drop_q} {}

int Migration::Start(int argc, char *argv[]) {
  if (argc != 2) {
    Usage(argv[0]);
    return EXIT_FAILURE;
  }

  const std::string action = argv[1];

  if (action == "migrate") {
    return Run(migrate_q_, "Migrating...");
  } else if (action == "rollback") {
    return Run(rollback_q_, "Rolling back...");
  } else if (action == "fixtures") {
    return Run(fixtures_q_, "Loading fixtures...");
  } else if (action == "drop") {
    return Run(drop_q_, "Dropping...");
  } else if (action == "show") {
    std::cout << "\033[32mMigrate query:\033[0m\n"
              << pigment(migrate_q_) << std::endl;
    std::cout << "\n\033[32mRollback query:\033[0m\n"
              << pigment(rollback_q_) << std::endl;
    return EXIT_SUCCESS;
  } else if (action == "help") {
    Usage(argv[0]);
    return EXIT_SUCCESS;
  } else {
    Usage(argv[0]);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

} // namespace yai::migration
