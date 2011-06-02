
namespace Util {

  enum Error {
    SUCCESS = 0,
    FAILURE,

    ALLOCATION_FALILURE,

    EMPTY_WORD,
    NO_WORD,

    DUPLICATE,
    CORRUPTION,

    NOT_FOUND,

    NO_PATH,
    EMPTY_PATH,
    PATH_TOO_LONG,
    PATH_NOT_FOUND,
    MALFORMED_PATH,

    NO_NAME,
    EMPTY_NAME,
    NAME_TOO_LONG,
    MALFOMED_NAME,

    NOT_A_DIR,
    NO_DIR,
  };

  const char * error_string(Error err)
  {
    switch (err) {
      case SUCCESS: return "success";
      case FAILURE: return "failure";
      case ALLOCATION_FALILURE: return "allocation failure";
      case EMPTY_WORD: return "empty word";
      case NO_WORD: return "no word";
      case DUPLICATE: return "duplicate";
      case CORRUPTION: return "corruption";
      case NOT_FOUND: return "not found";
      case NO_PATH: return "no path";
      case EMPTY_PATH: return "empty path";
      case PATH_TOO_LONG: return "path too long";
      case PATH_NOT_FOUND: return "path not found";
      case MALFORMED_PATH: return "malformed path";
      case NO_NAME: return "no name";
      case EMPTY_NAME: return "empty name";
      case NAME_TOO_LONG: return "name tool long";
      case MALFOMED_NAME: return "malformed name";
      case NOT_A_DIR: return "not a directory";
      case NO_DIR: return "no directory";
    }
    return 0;
  }
};
