#include <exception>
#include <string>

class TAprsdException: public exception {

private:
    string what_str;

public:
    TAprsdException(const string &what_arg) throw ():
        what_str(what_arg) { }
    virtual const char *what() const throw () {
        return what_str.c_str();
    }
    ~TAprsdException() throw () { }; // Required by compiler
};

