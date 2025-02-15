#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h"

class Config
{
  public:
    Config()
    {
        reload();
    }

    // Загружает конфигурацию из файла "settings.json"
    // Был добавлен фикс, разрешающий комментарии в json-файле,
    // для успешного выполнения п. 7 задания 
    void reload()
    {
        std::ifstream fin(project_path + "settings.json");
        // Разрешает комментарии в json-файле (последний параметр)
        config = json::parse(fin, nullptr, true, true);
        //fin >> config; // это прежний вариант чтения конфига
        fin.close();
    }

    // Оператор был определен для доступа к конкретной настройке из файла "settings.json".
    // Настройки разделены на "каталоги" (WindowSize, Bot, Game), в каждом из которых
    // находятся спецефичные настройки, доступные по названию.
    auto operator()(const string &setting_dir, const string &setting_name) const
    {
        return config[setting_dir][setting_name];
    }

  private:
    json config;
};
