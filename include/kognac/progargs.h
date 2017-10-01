#ifndef _PROGARGS_H
#define _PROGARGS_H

#include <kognac/logs.h>

#include <vector>
#include <set>
#include <map>

using namespace std;

class ProgramArgs {
    public:
        class AbsArg {
            private:
                string shortname;
                string name;
                string desc;
                bool isset;
                bool required;
            protected:
                AbsArg(string shortname,
                        string name,
                        string desc,
                        bool required) {
                    this->shortname = shortname;
                    this->name = name;
                    this->desc = desc;
                    this->isset = false;
                    this->required = required;
                }
                void markSet() {
                    isset = true;
                }
                template<class K>
                    bool check(string s);
                template<class K>
                    void convert(string s, K &v);
            public:
                bool isRequired() {
                    return required;
                }
                bool isSet() {
                    return isset;
                }
                string getName() {
                    return name;
                }
                virtual void set(string value) = 0;
                virtual string tostring() {
                    string out = "";
                    if (shortname == "") {
                        out += "--" + name + " arg ";
                    } else {
                        out += "-" + shortname + " OR --" + name + " arg ";
                    }
                    string padder = " ";
                    while (out.size() + padder.size() < 40) {
                        padder += " ";
                    }
                    string d = desc;
                    if (d.size() < 40) {
                        out += padder + d;
                    } else {
                        string d1 = d.substr(0, 40);
                        out += padder + d1 + "\n";
                        d = d.substr(40, d.size() - 40);
                        while (d.size() > 40) {
                            d1 = d.substr(0, 40);
                            for (int i = 0; i < 40; ++i) out += " ";
                            out += d1 + "\n";
                            d = d.substr(40, d.size() - 40);
                        }
                        for (int i = 0; i < 40; ++i) out += " ";
                        out += d;
                    }
                    out += " (";
                    if (required) {
                        out += "REQUIRED";
                    } else {
                        out += "OPTIONAL";
                    }
                    return out;
                }
        };
        template <class K>
            class Arg : public AbsArg {
                private:
                    K value;
                public:
                    Arg(K defvalue,
                            string shortname,
                            string name,
                            string desc,
                            bool required) : AbsArg(shortname, name, desc, required) {
                        this->value = defvalue;
                    }
                    void set(string value) {
                        if (isSet()) {
                            LOG(ERROR) << "Param '" << getName() << "' cannot be set more than once";
                            throw 10;
                        }
                        if (!check<K>(value)) {
                            LOG(ERROR) << "Value for the param '" << getName() << "' cannot be converted to the appropriate value";
                            throw 10;
                        }
                        convert<K>(value, this->value);
                        markSet();
                    }
                    K get() {
                        return value;
                    }
                    string tostring() {
                        string out = AbsArg::tostring();
                        if (!isRequired()) {
                            std::stringstream ss;
                            ss << value;
                            out += " DEFAULT=" + ss.str() + ")";
                        } else {
                            out += ")";
                        }
                        return out;
                    }
            };
        class OutVar {
            private:
                std::shared_ptr<AbsArg> v;
            public:
                OutVar(std::shared_ptr<AbsArg> v) : v(v) {}
                template<class K>
                    K as() {
                        return ((Arg<K>*)v.get())->get();
                    }
                bool empty() {
                    return !v->isSet();
                }
        };
        class GroupArgs {
            private:
                string namegroup;
                map<string, std::shared_ptr<AbsArg>> &shortnames;
                map<string, std::shared_ptr<AbsArg>> &names;
                std::vector<std::shared_ptr<AbsArg>> args;

                void addNewName(string sn, string n,
                        std::shared_ptr<AbsArg> arg) {
                    if (sn != "") {
                        if (!shortnames.count(sn)) {
                            shortnames.insert(make_pair(sn, arg));
                        } else {
                            LOG(ERROR) << "Parameter " << sn << " already defined";
                            throw 10;
                        }
                    }
                    if (!names.count(n)) {
                        names.insert(make_pair(n, arg));
                    } else {
                        LOG(ERROR) << "Parameter " << n << " already defined";
                        throw 10;
                    }
                }

            public:
                GroupArgs(string name, map<string, std::shared_ptr<AbsArg>> &shortnames,
                        map<string, std::shared_ptr<AbsArg>> &names) : namegroup(name), shortnames(shortnames), names(names) {
                }

                template<class K>
                    GroupArgs& add(string shortname, string name, K defvalue,
                            string desc, bool required) {
                        auto p = std::shared_ptr<AbsArg>(
                                new Arg<K>(defvalue, shortname, name, desc, required));
                        addNewName(shortname, name, p);
                        args.push_back(p);
                        return *this;
                    }
                string tostring() {
                    string out = "";
                    for(auto params : names) {
                        out += params.second->tostring() + "\n";
                    }
                    out += "\n";
                    return out;
                }
        };

    private:
        map<string, std::shared_ptr<AbsArg>> shortnames;
        map<string, std::shared_ptr<AbsArg>> names;
        std::map<string, std::shared_ptr<GroupArgs>> args;
        std::set<string> posArgs;

    public:
        std::shared_ptr<GroupArgs> newGroup(string name) {
            auto ptr = std::shared_ptr<GroupArgs>(new GroupArgs(name, shortnames, names));
            args.insert(make_pair(name, ptr));
            return ptr;
        }
        void parse(int argc, const char** argv) {
            for(int i = 0; i < argc; ++i) {
                if (argv[i][0] == '-' && argv[i][1] == '-') {
                    string nameParam = string(argv[i]).substr(2);
                    if (names.count(nameParam)) {
                        if (i == argc - 1) {
                            LOG(ERROR) << "Miss value of the param";
                            throw 10;
                        }
                        string value = argv[++i];
                        names[nameParam]->set(value);
                    } else {
                        LOG(ERROR) << "Param " << nameParam << " not found";
                        throw 10;
                    }
                } else if (argv[i][0] == '-') {
                    string nameParam = string(argv[i]).substr(1);
                    if (shortnames.count(nameParam)) {
                        if (i == argc - 1) {
                            LOG(ERROR) << "Miss value of the param";
                            throw 10;
                        }
                        string value = argv[++i];
                        shortnames[nameParam]->set(value);
                    } else {
                        LOG(ERROR) << "Param " << nameParam << " not found";
                        throw 10;
                    }
                } else {
                    posArgs.insert(string(argv[i]));
                }
            }
        }
        void check() {
            for(auto pair : names) {
                if (pair.second->isRequired() && !pair.second->isSet()) {
                    LOG(ERROR) << "Param " << pair.second->getName() << " is required but no set";
                    throw 10;
                }
            }
        }
        OutVar operator [](string n) {
            if (!names.count(n)) {
                LOG(ERROR) << "Param " << n << " not found";
                throw 10;
            }
            return OutVar(names.find(n)->second);
        }
        bool count(string name) {
            return posArgs.count(name) ||
                (names.count(name) && names.find(name)->second->isSet());
        }

        string tostring() const {
            string out = "";
            for (auto pair : args) {
                out += pair.first + ":\n";
                out += pair.second->tostring();
            }
            return out;
        }
};
#endif
