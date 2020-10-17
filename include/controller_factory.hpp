#pragma once
#include "controller.hpp"

#define INIT_CONTROLLER_REGISTRY() \
  std::shared_ptr<ControllerFactory::map_type> ControllerFactory::map = nullptr;

#define REGISTER_CONTROLLER(name) ControllerRegister<name> name::reg(#name);

template<typename T> 
std::shared_ptr<Controller::Instance> createT(std::string name, std::string views_path) {
  return std::make_shared<T>(name, views_path); 
}

template<typename T> 
void routesT(Pistache::Rest::Router& router, std::shared_ptr<Controller::Instance> controller) { 
  T::Routes(router, controller); 
}

struct ControllerMapEntry{
  public:
    std::shared_ptr<Controller::Instance>(*constructor)(std::string, std::string);
    void (*routes)(Pistache::Rest::Router&, std::shared_ptr<Controller::Instance>);
};

struct ControllerFactory {
  typedef std::map<std::string, ControllerMapEntry> map_type;

  public:
    static std::shared_ptr<Controller::Instance>
    createInstance(std::string const& s, std::string views_path) {

      map_type::iterator it = getMap()->find(s);
      if(it == getMap()->end())
        throw std::runtime_error("Controller "+s+" not found");
      return it->second.constructor(s, views_path);
    }

    static void setRoutes(std::string const &s, Pistache::Rest::Router& router, std::shared_ptr<Controller::Instance> controller) {
      map_type::iterator it = getMap()->find(s);
      // I guess we just silently die if the controller wasn't found...
      if(it == getMap()->end()) 
        throw std::runtime_error("Controller "+s+" not found");

      it->second.routes(router, controller);
    }

    static std::vector<std::string> getControllerNames() {
      std::vector<std::string> ret; 
      auto m = ControllerFactory::getMap();
      transform(m->begin(), m->end(), back_inserter(ret), [](auto& c) { 
        return c.first; });
      return ret;
    }

  protected:
    static std::shared_ptr<map_type> getMap() {
      if(!map) { map = std::make_shared<map_type>(); } 
      return map; 
    }

  private:
    static std::shared_ptr<map_type> map;
};

template<typename T>
struct ControllerRegister : ControllerFactory { 
  ControllerRegister(std::string const& s) { 
    ControllerMapEntry me;
    me.constructor = &createT<T>;
    me.routes = &routesT<T>;

    getMap()->insert(std::make_pair(s, me));
  }
};

