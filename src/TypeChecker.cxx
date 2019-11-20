#include "TypeChecker.h"
#include "ASTNode.h"
#include "Messages.h"
#include <vector>
#include <string>
#include <stdio.h>
#include <map>

namespace TypeChecker {
    //define structs for managing definitions
    enum isInit {
      unseen, no, yes, unknown  
    };
    //There isn't really a way to define which node a variable references, since it appears multiple times
    struct Var {
        std::string name;
        std::string type;
        isInit init = unseen;
    };
    struct Method {
        AST::Method * node;
        std::string name;
        std::vector<struct Var *> * arguments;
        std::map<std::string, struct Var *> *table;
        std::string returnType;
    };
    struct Class {
        AST::Class * node;
        std::string name;
        std::string super;
        struct Method *constructor;
        std::map<std::string, struct Method *> *methods;
        std::map<std::string, struct Var *> *fields;
    } Obj, Int, String, Boolean;
    //Note that you cant have methods outside of a class... So we won't use this methods
    std::map<std::string, struct Class *> classes;
    std::map<std::string, struct Var *> variables;

    //Vars can be instantiated with type defined or not... If not it will be the empty string
    struct Var * createVar(AST::Ident * name, AST::Ident * type) {
        struct Var * output = new struct Var();
        output->name = name->getText();
        if(type == NULL) {
            output->type = "";
        } else {
            output->type = type->getText();
        }
        return output;
    }
    struct Method * createMethod(AST::Method *method) {
        struct Method * output = new struct Method();
        output->name = method->getName()->getText();
        output->node = method;

        std::map<std::string, struct Var *> *variables = new std::map<std::string, struct Var *>();
        std::vector<Var *> *args = new std::vector<Var *>();
        for(auto argument : method->getFormals()->getElements()) {
            struct Var * var = new struct Var();
            var->name = dynamic_cast<AST::Ident *>(argument->getName())->getText();
            var->type = dynamic_cast<AST::Ident *>(argument->getType())->getText();
            args->push_back(var);
            variables->insert({var->name, var});
        }
        output->arguments = args;
        output->table = variables;

        output->returnType = method->getReturnType()->getText();
        return output;
    }
    int compareMethods(struct Method * one, struct Method * two) {
        //same number of arguments?
        if(one->arguments->size() != two->arguments->size()) {
            //report::error("Method " + one->name + " does not have the same number of arguments as " + two->name);
            return -1;
        }
        //Now we can check argument type
        for(int i = 0; i < one->arguments->size(); i++) {
            if (one->arguments->at(i)->type.compare(two->arguments->at(i)->type) != 0) {
                //report::error("Arguments of " + one->name + " and " + two->name + " do not have the same type");
                return -1;
            }
        }
        if(one->returnType.compare(two->returnType) != 0) {
            return -1;
        }
        return 0;
    }
    int compareVar(struct Var * one, struct Var * two) {
        if(one->type.compare(two->type) != 0) {
            return -1;
        }
        if(one->init != two->init) {
            return -2;
        }
        return 0;
    }
    struct Class * createClass(AST::Class * clazz) {
        struct Class * output = new struct Class();
        output->node = clazz;
        output->name = clazz->getName()->getText();
        output->super = clazz->getSuper()->getText();
        //The statements associated with a class actually fall into the constructor technically
        output->constructor = createMethod(clazz->getConstructor());

        std::map<std::string, struct Var *> *fields = new std::map<std::string, struct Var *>();
        output->fields = fields;

        std::map<std::string, struct Method *> *methods = new std::map<std::string, struct Method *>();
        for(auto method : clazz->getMethods()->getElements()) {
            std::map<std::string, struct Method *>::iterator it = methods->find(method->getName()->getText());
            if(it != methods->end()) {
                report::error("Duplicate method name of " + method->getName()->getText() + " in class " + output->name);
                exit(16);
            }
            methods->insert({method->getName()->getText(), createMethod(method)});
        }

        output->methods = methods;
        return output;
    }
    //BEGIN HELPER METHODS TO BE REMOVED
    std::string argsInLine(std::vector<struct Var *> *args) {
        std::string output = "(";
        for(auto arg : *args) {
            output = output + arg->name + ":" + arg->type + ", ";
        }
        output = output + ")";
        return output;
    }
    std::string printVariablesInLine(std::map<std::string, struct Var *> *variables) {
        std::string output = "{";
        for(const auto& pair : *variables) {
            struct Var * var = pair.second;
            output = output + var->name + ":" + var->type + "  ";
        }
        return (output = output + "}");
    }
    void printClasses() {
        for(const auto& pair : classes) {
            if(pair.first.compare("Obj") == 0 || pair.first.compare("Int") == 0 ||
                pair.first.compare("String") == 0 || pair.first.compare("Boolean") == 0) {
                    continue;
            } else {
                std::cout << pair.first << ":" << std::endl;
                std::cout << "  Constructor (" << argsInLine(pair.second->constructor->arguments) << ")" << std::endl;
                std::cout << "  Fields: " << printVariablesInLine(pair.second->fields) << std::endl;
                for(const auto& pair2 : *pair.second->methods) {
                    std::cout << "  " << pair2.first << " (" << argsInLine(pair2.second->arguments) << ") : " << pair2.second->returnType << std::endl;
                    std::cout << "      " << printVariablesInLine(pair2.second->table) << std::endl;
                } 
            }
        }
    }

    //this returns a 'location'
    struct Var * processLexpr(struct Class * clazz, struct Method * method, AST::LExpr * lExpr) {
        if(lExpr->getType() == AST::lexprTypes::IDENT) {
            std::cout << "Found lexpr" << std::endl;

            std::string name = ((AST::Ident *)lExpr)->getText();
            //check if it already exists in table
            std::map<std::string, struct Var *>::iterator it = method->table->find(name);
            if (it == method->table->end()) {
                struct Var * output = new struct Var();
                output->name = name;
                output->init = no;
                method->table->insert({output->name, output});
                return output;
            } else {
                //ie it does already exist
                return it->second;
            }
        }else if (lExpr->getType() == AST::lexprTypes::DOT) {
            //if left side is this and clazz is null error out since that means we are in the global scope
            AST::Dot * dot = dynamic_cast<AST::Dot *>(lExpr);
            std::cout << "Found Dot assign" << std::endl;
            std::string name = dot->getRight()->getText();
            //now we need to resolve the location
            struct Var * location = parseExpr(clazz, method, dot->getLeft());
            //This should result in a var that represents the classname of something
            

        }
        return 0;
    }
    //END HELPER METHODS TO BE REMOVED
    //This is run in the methods to parse the statments. It also acts as type inference. Needs reference to method, class, and statemtn
    struct Var * parseExpr(struct Class * clazz, struct Method * method, AST::Statement * stat) {
        std::cout << "Entering the parsing statments method" << std::endl;
        //we will have a seperate case for ASSIGN,ASSIGNDECLARE,EXPR,RETURN,IF,WHILE,LOAD,INTCONST,STRINGCONST,CALL,OOF
        if(stat->getType() == AST::statementEnum::INTCONST) {
            std::cout << "Found an Int constant" << std::endl;
            struct Var * output = new struct Var();
            output->type = "Int";
            output->name = "Nothing";
            return output;
        }else if(stat->getType() == AST::statementEnum::STRINGCONST) {
            std::cout << "Found an String constant" << std::endl;
            struct Var * output = new struct Var();
            output->type = "Str";
            output->name = "Nothing";
            return output;
        }else if(stat->getType() == AST::statementEnum::ASSIGN) {
            std::cout << "Found an Assign!" << std::endl;
            //ASSIGN is the most basic case. The left side variable gets the type of the right side. Lexpr can be either a local variable or a field
            //First we get the type from the rexpr
            //Then we resolve the location and name of the lexpr and assign it the type
            AST::Assign *assi = dynamic_cast<AST::Assign *>(stat); 
            std::string type = parseExpr(clazz, method, assi->getExpr())->type;
            struct Var * location = processLexpr(clazz, method, assi->getLexpr());

            if(location->init == no) {
                location->type = type;
                location->init = yes;
            }else if (location->init == yes) {
                if (location->type.compare(type) != 0) {
                    report::error("Type mismatch variable " + location->name + " has type " + location->type + " so you cannot assign a " + type);
                    exit(64);
                } //we dont need to do anything else right now since it is already the right type
            }

            return NULL;
        }else if(stat->getType() == AST::statementEnum::EXPR) {
            std::cout << "Found an expression" << std::endl;
        }else if(stat->getType() == AST::statementEnum::OOF) {
            std::cout << "Found an oof" << std::endl;
        }

        return NULL;
    }

    int parseMethod(struct Class * parent, struct Method * input) {
        /*
        1) Make sure init before use
        2) Fields need to be copied back to class. They are only considered initialized if in the constructor
        */
        bool constructor = false;
        if(parent->name.compare(input->name) == 0) {
            //This is the constructor then
            constructor = true;
        }

        //parse all of the statements
        for(auto node : input->node->getStatements()->getElements()) {
            AST::Statement * statement = dynamic_cast<AST::Statement *>(node);
            parseExpr(parent, input, statement);
        }

        return 0;
    }
    //Method to parse class
    int parseClass(struct Class * input) {
        std::cout << "CLass: " << input->name << " Super: " << input->super << std::endl;
        //When pulling the methods from the super lets make sure they don't overlap
        //If they do overlap it must be the same signature!!
        for(const auto& pair : *classes[input->super]->methods) {
            std::map<std::string, struct Method *>::iterator it = input->methods->find(pair.first);
            if(it == input->methods->end()) {
                //We don't have this method! Inherit it!
                input->methods->insert(pair);
            } else {
                //We do have this method... Make sure the signature matches!
                if(compareMethods(it->second, pair.second) == -1) {
                    report::error("Method " + it->first + " attempts to override superclass method but doesn't match signatures...");
                    exit(16);
                }
            }
        }

        //Lets instantiate the class scope for this class. Firstly is the constructor since it may initialize important instance variables
        parseMethod(input, input->constructor);

        //After parsing the constructor lets make sure we have all of the same fields as our parent
        for(const auto& pair : *classes[input->super]->fields) {
            std::map<std::string, struct Var *>::iterator it = input->fields->find(pair.first);
            if(it != input->fields->end()) {
                //ie we do have the field
                int comp = compareVar(it->second, pair.second);
                if(comp == -1) {
                    report::error("Type error in fields Class " + input->name + " and super " + input->super + " in variable " + pair.first);
                    exit(16);
                }else if (comp == -2) {
                    report::error("Field " + pair.first + " in class " + input->name + " super " + input->super + " does not have the same initialization level");
                    exit(32);
                }
            }
        }

        //Then all other methods since they may use instance variables
        for(const auto& pair : *input->methods) {
            //Make sure none of the methods match a field name at this point
            std::map<std::string, struct Var *>::iterator it = input->fields->find(pair.first);
            if(it != input->fields->end()) {
                report::error("Method " + pair.first + " shares a name with a field in " + input->name);
                exit(32);
            }
            struct Method * method = pair.second;
            parseMethod(input, method);
        }

        return 0;
    }
    //We want to check types, duplicate names between classess/methods/variables, methods, class super signatures, and init before use    
    int Check(AST::ASTNode *root_) {
        //caste the root
        AST::Program *root = dynamic_cast<AST::Program *>(root_);   
        //we need the base classes for Obj, Int, String, Bool as well as the base methods that these base classes have as well
        Obj = {NULL, "Obj", "Nothing", NULL, new std::map<std::string, struct Method *>(), new std::map<std::string, struct Var *>()};

        std::map<std::string, struct Method *> * intMethods = new std::map<std::string, struct Method *>(); //TOADD METHODS
        Int = {NULL, "Int", "Obj", NULL, intMethods, NULL};
        std::map<std::string, struct Method *> * stringMethods = new std::map<std::string, struct Method *>();
        String = {NULL, "String", "Obj", NULL, stringMethods, NULL};
        std::map<std::string, struct Method *> * booleanMethods = new std::map<std::string, struct Method *>();
        Boolean = {NULL, "Boolean", "Obj", NULL, booleanMethods, NULL};
        

        //Create the methods for them Str(), Plus() etc

        //Now we will create our reference of classes to check and the valid types
        classes.insert({"Obj", &Obj});
        classes.insert({"Int", &Int});
        classes.insert({"String", &String});
        classes.insert({"Boolean", &Boolean});
        for(auto clazz : root->classes_.getElements()) {
            classes.insert({clazz->getName()->getText(), createClass(clazz)});
        }

        //lets get the classes in topological order (sort of)
        //This algorithm is probably very inefficient for large numbers of strings but hey... Who's writing a 2000 class Quack program please don't test this with one Prof Young ;)
        std::vector<std::string> topo;
        topo.push_back(Obj.name);
        bool change = true;
        while(change) {
            change = false;
            //loop through all of the classes adding them to the topo vector only if their super is already in it
            for(const auto& pair : classes) {
                if (std::find(topo.begin(), topo.end(), pair.second->name) != topo.end()) {
                    continue;
                } else {
                    if (std::find(topo.begin(), topo.end(), pair.second->super) != topo.end()) {
                        topo.push_back(pair.second->name);
                        change = true;
                        //once nothing new needs to be added we are done
                    }else {
                        continue;
                    }
                }
            }
        }

        //Now we have our ''Global'' scope done. A reference to all of the classes added so we can use their constructors

        //Now lets parse those classes! 
        for(auto name : topo) {
            struct Class * clazz = classes[name];
            if(clazz->name.compare("Obj") == 0 || clazz->name.compare("String") == 0 ||
                clazz->name.compare("Int") == 0 || clazz->name.compare("Boolean") == 0) {
                    continue;
                }
            parseClass(clazz);
        }

        std::map<std::string, struct Var *> *scopeVariables = new std::map<std::string, struct Var *>();
        struct Method fakeMethod = {NULL, "ProgramLevelFakeMethod", NULL, scopeVariables, "Nothing"};
        for(auto node : root->statements_.getElements()) {
            AST::Statement * statement = dynamic_cast<AST::Statement *>(node);
            parseExpr(NULL, &fakeMethod,  statement);
        }

        printClasses();
        std::cout << "Variables in max scope " << printVariablesInLine(scopeVariables) << std::endl;

        return 0;
    }
}