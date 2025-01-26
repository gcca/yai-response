#include <yai-migration.hpp>

static const char *migrate_q = R"(DO $$
BEGIN
  CREATE TABLE yai_booking_consultant (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL
  );

  CREATE TABLE yai_booking_customer (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL
  );

  CREATE TABLE yai_booking_book (
    id SERIAL PRIMARY KEY,
    consultant_id INT NOT NULL,
    customer_id INT NOT NULL,
    visited_at TIMESTAMP NOT NULL,
    comment TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (consultant_id) REFERENCES yai_booking_consultant (id),
    FOREIGN KEY (customer_id) REFERENCES yai_booking_customer (id)
  );

  CREATE INDEX idx_yai_booking_book_created_at ON yai_booking_book USING btree (created_at);

  CREATE TABLE yai_booking_message(
    id SERIAL PRIMARY KEY,
    message TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
  );
END $$;
)";

static const char *rollback_q = R"(DO $$
BEGIN
  DELETE FROM yai_booking_book;
  DELETE FROM yai_booking_customer;
  DELETE FROM yai_booking_consultant;
  DELETE FROM yai_booking_message;
END $$;)";

static const char *fixtures_q = R"(DO $$
BEGIN
  INSERT INTO yai_booking_consultant (name) VALUES
    ('Bruce Wayne'), ('Clark Kent'), ('Diana Prince'), ('Barry Allen'), ('Arthur Curry');

  INSERT INTO yai_booking_customer (name) VALUES
    ('Tony Stark'), ('Steve Rogers'), ('Natasha Romanoff'), ('Bruce Banner'), ('Thor Odinson');

  INSERT INTO yai_booking_book (consultant_id, customer_id, comment) VALUES
    (1, 1, 'Need to discuss about the new suit'), (2, 2, 'Need to discuss about the new shield'),
    (3, 3, 'Need to discuss about the new lasso'), (4, 4, 'Need to discuss about the new shoes'),
    (5, 5, 'Need to discuss about the new trident');
END $$;)";

static const char *drop_q = R"(DO $$
BEGIN
  DROP TABLE IF EXISTS yai_booking_book CASCADE;
  DROP TABLE IF EXISTS yai_booking_customer CASCADE;
  DROP TABLE IF EXISTS yai_booking_consultant CASCADE;
  DROP TABLE IF EXISTS yai_booking_message CASCADE;
END $$;)";

int main(int argc, char *argv[]) {
  yai::migration::Migration migration{migrate_q, rollback_q, fixtures_q,
                                      drop_q};

  return migration.Start(argc, argv);
}
