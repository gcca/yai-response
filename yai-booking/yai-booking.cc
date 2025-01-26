#include <iostream>

#include "yai-booking-handlers.hpp"

static yai::Handler handlers[] = {
    yai::booking::handlers::ListConsultants,
    yai::booking::handlers::ImportCSV,
};

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;

    return EXIT_FAILURE;
  }

  try {
    yai::Server(yai::utils::StoPortNum(argv[1]), handlers).Run();
  } catch (std::exception &e) {
    std::cerr << "Runtime error: " << e.what() << std::endl;
    throw;
  }

  return EXIT_SUCCESS;
}
