  bool SwissSECODatabase::setup()
  {

    ifstream inFILE(getURI() + "consolidated-list.xml", ios::in | ios::binary);

    if (!inFILE)
    {
      return false;
    }

    ptree pt;
    try
    {
      read_xml(inFILE, pt);
    }
    catch (const exception &e)
    {
      cerr << "Error encountered while parsing XML for " << this->getName() << ": " << e.what() << '\n';
      return false;
    }

    // Assemble references

    unordered_map<string, string> locations;
    unordered_map<string, string> sanctions_map;

    for (auto &entry : pt.get_child("swiss-sanctions-list"))
    {

      if (entry.first == "sanctions-program")
      {

        string program;
        string sanction;
        string setID;

        for (auto &sub_entry : entry.second)
        {
          if ((sub_entry.first == "sanctions-set") &&
              (sub_entry.second.get<string>("<xmlattr>.lang") == "eng"))
          {
            program = sub_entry.second.data();
            setID = sub_entry.second.get<string>("<xmlattr>.ssid");
          }
          else if ((sub_entry.first == "program-name") &&
                   (sub_entry.second.get<string>("<xmlattr>.lang") == "eng"))
          {
            sanction = sub_entry.second.data();
          }
        }

        sanctions_map[setID] = program + ". " + sanction;
      }
      else if (entry.first == "place")
      {

        string area;
        string region;
        string country;

        try
        {
          region = entry.second.get<string>("location");
        }
        catch (...)
        {
        }

        try
        {
          area = entry.second.get<string>("area");
        }
        catch (...)
        {
        }

        try
        {
          country = entry.second.get<string>("country");
        }
        catch (...)
        {
        }

        string location = region;

        if (!area.empty())
        {
          if (!location.empty())
          {
            location += ", ";
          }
          location += area;
        }

        if (!country.empty())
        {
          if (!location.empty())
          {
            location += ", ";
          }
          location += country;
        }

        locations[entry.second.get<string>("<xmlattr>.ssid")] = location;
      }
    }

    vector<string> IDs;

    for (auto &entry : pt.get_child("swiss-sanctions-list"))
    {

      if (entry.first == "target")
      {

        entries.emplace_back();
        EntityData &newEntry = entries.back();

        newEntry.sources.emplace_back(name);

        try
        {
          newEntry.sanctions.emplace_back(sanctions_map.at(entry.second.get<string>("<xmlattr>.sanctions-set-id")));
        }
        catch (...)
        {
        }

        try
        {

          ptree &sub_pt = entry.second.get_child("individual");

          newEntry.type = EntityType::Individual;

          for (auto &attr : sub_pt)
          {

            if (attr.first == "justification")
            {
              if (!hyperC::anyEqual(attr.second.data(), newEntry.notes))
              {
                newEntry.notes.emplace_back(attr.second.data());
              }
            }
            else if (attr.first == "other-information")
            {
              if (!hyperC::anyEqual(attr.second.data(), newEntry.notes))
              {
                newEntry.notes.emplace_back(attr.second.data());
              }
            }
          }

          for (auto &attr : sub_pt.get_child("identity"))
          {
            if (attr.first == "name")
            {

              Name *newName = nullptr;

              if (attr.second.get<string>("<xmlattr>.name-type") == "primary-name")
              {
                newName = &newEntry.name;
              }
              else
              {
                newEntry.aliases.emplace_back();
                newName = &newEntry.aliases.back();
              }

              vector<string> name_parts;
              vector<string> titles;

              bool bTitle = false;

              for (auto &part : attr.second)
              {

                if (part.first == "name-part")
                {

                  size_t part_order = part.second.get<int>("<xmlattr>.order");
                  string part_type = part.second.get<string>("<xmlattr>.name-part-type");
                  std::string part_value = part.second.get<string>("value");

                  if (!part_value.empty() && (part_value != "-"))
                  {
                    if (part_type != "title")
                    {
                      if (name_parts.size() < part_order)
                      {
                        name_parts.resize(part_order);
                      }
                      name_parts[part_order - 1] = part_value;
                    }
                    else
                    {
                      titles.emplace_back(part_value);
                    }
                  }
                }
              }

              std::string nameStr;

              if (!name_parts.empty())
              {
                for (const auto &str : name_parts)
                {
                  if (!nameStr.empty())
                  {
                    nameStr += " ";
                  }
                  nameStr += str;
                }
              }
              else if (!titles.empty())
              {

                for (const auto &str : titles)
                {
                  if (!nameStr.empty())
                  {
                    nameStr += " ";
                  }
                  nameStr += str;
                }
              }

              *newName = nameStr;
            }
            else if (attr.first == "nationality")
            {
              newEntry.nationality = attr.second.get<string>("country");
            }
            else if (attr.first == "day-month-year")
            {

              SourcedTimePoint *newTime = nullptr;

              if (newEntry.TimeBegin.empty())
              {
                newTime = &newEntry.TimeBegin;
              }
              else
              {
                newEntry.altTimes.emplace_back();
                newTime = &newEntry.altTimes.back();
              }

              try
              {
                newTime->year() = attr.second.get<int>("<xmlattr>.year");
                newTime->month() = attr.second.get<int>("<xmlattr>.month");
                newTime->day() = attr.second.get<int>("<xmlattr>.day");
              }
              catch (...)
              {
              }
            }
            else if (attr.first == "address")
            {
              newEntry.locations.emplace_back(locations.at(attr.second.get<string>("<xmlattr>.place-id")));
            }
            else if (attr.first == "place-of-birth")
            {
              newEntry.POB = locations.at(attr.second.get<string>("<xmlattr>.place-id"));
            }
            else if (attr.first == "identification-document")
            {

              newEntry.ID[capitalized(attr.second.get<string>("<xmlattr>.document-type"))] =
                  attr.second.get<string>("number");

              if (attr.second.get<string>("<xmlattr>.document-type") == "passport")
              {
                newEntry.passports.emplace_back();
                newEntry.passports.back().ID = attr.second.get<string>("number");

                try
                {
                  newEntry.passports.back().nationality = attr.second.get<string>("issuer");
                }
                catch (...)
                {
                }

                try
                {
                  string date_of_issue = attr.second.get<string>("date-of-issue");

                  vector<string> parse;
                  splitString(date_of_issue, parse, "-");

                  newEntry.passports.back().issued.year() = stoi(parse.at(0));
                  newEntry.passports.back().issued.month() = stoi(parse.at(1));
                  newEntry.passports.back().issued.day() = stoi(parse.at(2));
                }
                catch (...)
                {
                }
              }
            }
          }
        }
        catch (...)
        {

          try
          {

            ptree &sub_pt = entry.second.get_child("entity");

            newEntry.type = EntityType::Organization;

            for (auto &attr : sub_pt)
            {

              if (attr.first == "justification")
              {
                if (!hyperC::anyEqual(attr.second.data(), newEntry.notes))
                {
                  newEntry.notes.emplace_back(attr.second.data());
                }
              }
              else if (attr.first == "other-information")
              {
                if (!hyperC::anyEqual(attr.second.data(), newEntry.notes))
                {
                  newEntry.notes.emplace_back(attr.second.data());
                }
              }
            }

            for (auto &attr : sub_pt.get_child("identity"))
            {

              if (attr.first == "name")
              {
                Name *newName = nullptr;

                if (attr.second.get<string>("<xmlattr>.name-type") == "primary-name")
                {
                  newName = &newEntry.name;
                }
                else
                {
                  newEntry.aliases.emplace_back();
                  newName = &newEntry.aliases.back();
                }

                newName->assign(attr.second.get<string>("name-part.value"));
              }
            }
          }
          catch (...)
          {
          }
        }

        IDs.emplace_back(entry.second.get<string>("<xmlattr>.ssid"));
      }
    }

    // Assemble links

    size_t i = 0;
    size_t j = 0;
    size_t L = entries.size();

    for (auto &entry : pt.get_child("swiss-sanctions-list"))
    {
      if (entry.first == "target")
      {

        string in_ID = entry.second.get<string>("<xmlattr>.ssid");

        try
        {
          for (auto &field : entry.second.get_child("individual"))
          {
            if (field.first == "relation")
            {

              string out_ID = field.second.get<string>("<xmlattr>.target-id");
              string type = field.second.get<string>("<xmlattr>.relation-type");

              entries[i].linkedTo.emplace_back();

              for (j = 0; j < L; ++j)
              {
                if (IDs[j] == out_ID)
                {
                  entries[i].linkedTo.back() = entries[j].name;
                  entries[i].linkedTo.back().setLinkType(type);
                  break;
                }
              }
            }
          }
        }
        catch (...)
        {
          try
          {
            for (auto &field : entry.second.get_child("entity"))
            {

              if (field.first == "relation")
              {

                string out_ID = field.second.get<string>("<xmlattr>.target-id");
                string type = field.second.get<string>("<xmlattr>.relation-type");

                entries[i].linkedTo.emplace_back();

                for (j = 0; j < L; ++j)
                {
                  if (IDs[j] == out_ID)
                  {
                    entries[i].linkedTo.back() = entries[j].name;
                    entries[i].linkedTo.back().setLinkType(type);
                    break;
                  }
                }
              }
            }
          }
          catch (...)
          {
          }
        }
      }

      ++i;
    }

    return !entries.empty();
  }
