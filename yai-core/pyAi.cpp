#include <iostream>

#include "pyAi.hpp"

namespace {
namespace conio {

inline static void Danger(const char *message) {
  std::cerr << "\033[31mDanger: " << message << "\033[0m" << std::endl;
}

inline static void Warning(const char *message) {
  std::cerr << "\033[33mWarning: " << message << "\033[0m" << std::endl;
}

} // namespace conio
} // namespace

namespace pyAi {

bool InitSettings(ABISettings &abi_settings) {
  PyObject *dj_conf = PyImport_ImportModule("django.conf");

  if (!dj_conf) {
    PyErr_SetString(PyExc_RuntimeError, "Error importing django.conf");
    return true;
  }

  PyObject *settings = PyObject_GetAttrString(dj_conf, "settings");

  if (!settings) {
    PyErr_SetString(PyExc_RuntimeError, "Error getting settings");
    return true;
  }

  PyObject *xai_api_key = PyObject_GetAttrString(settings, "XAI_API_KEY");
  if (xai_api_key) {
    if (xai_api_key == Py_None) {
      conio::Danger("XAI_API_KEY is None");
      Py_DECREF(xai_api_key);
    } else {
      abi_settings.xai_api_key = PyUnicode_AsUTF8(xai_api_key);

      if (!abi_settings.xai_api_key) {
        conio::Danger("XAI_API_KEY is empty");
      }

      if (strnlen(abi_settings.xai_api_key, 32) == 0) {
        conio::Danger("XAI_API_KEY is too short");
      }

      Py_DECREF(xai_api_key);
    }
  } else {
    conio::Danger("XAI_API_KEY not found in settings");
  }

  PyObject *xai_model = PyObject_GetAttrString(settings, "XAI_MODEL");
  if (xai_model) {
    abi_settings.xai_model = PyUnicode_AsUTF8(xai_model);
    Py_DECREF(xai_model);
  } else {
    static const char *DEFAULT_MODEL = "grok-2-1212";
    abi_settings.xai_model = DEFAULT_MODEL;
  }

  PyObject *conninfo = PyObject_GetAttrString(settings, "YAI_ABI_CONNINFO");

  if (conninfo) {
    abi_settings.conninfo = PyUnicode_AsUTF8(conninfo);
    Py_DECREF(conninfo);
  } else {
    conio::Warning("YAIB_ABI_CONNINFO not found in settings");
    static const char *DEFAULT_CONNINFO = "dbname=yai user=postgres";
    abi_settings.conninfo = DEFAULT_CONNINFO;
  }

  Py_DECREF(settings);
  Py_DECREF(dj_conf);

  return false;
}

} // namespace pyAi
