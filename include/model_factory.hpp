#pragma once
#include "model.hpp"

#define INIT_MODEL_REGISTRY() \
  std::shared_ptr<ModelFactory::map_type> ModelFactory::map = nullptr;

#define REGISTER_MODEL(name) ModelRegister<name> name::reg(#name);

template<typename T> 
void migrateT() { 
  T::Migrate(); 
}

struct ModelMapEntry{
  public:
    void (*migrate)();
};

struct ModelFactory {
  typedef std::map<std::string, ModelMapEntry> map_type;

  public:

    static void migrate(std::string const& s) {
      map_type::iterator it = getMap()->find(s);
      // I guess we just silently die if the model wasn't found...
      if(it == getMap()->end()) return; 

      it->second.migrate();
    }

    static std::vector<std::string> getRegistrations() {
      std::vector<std::string> ret; 
      auto m = ModelFactory::getMap();
      for(auto it = m->begin(); it != m->end(); it++) ret.push_back(it->first);
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
struct ModelRegister : ModelFactory { 
  ModelRegister(std::string const& s) { 
    ModelMapEntry me;
    me.migrate = &migrateT<T>;

    getMap()->insert(std::make_pair(s, me));
  }
};

