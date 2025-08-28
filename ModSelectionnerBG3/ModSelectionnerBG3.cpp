#include <iostream>
#include <filesystem>
#include <fstream>
#include <windows.h>
#include <shlobj.h>
#include "nfd.h"
#include "Tools.h"
#include "JSON/json.hpp"

using namespace nlohmann;

constexpr const char* SettingFileName = "Profile.ini";
constexpr const char* ProfilesHolderName = "Profiles";
constexpr const char* SettingsHolderName = "Settings";
constexpr const char* ModsListSettingsPath = "PlayerProfiles\\Public\\modsettings.lsx";
constexpr const char* ModListFilename = "modsettings.lsx";
constexpr const char* InvalidProfileName = "-1";
constexpr int Indent = 4;

struct Settings
{
	std::string exec_mods_folder_path;
	std::string mods_storage_path;
};

struct Profile
{
	std::string name = InvalidProfileName;
	std::string access_path = InvalidProfileName;
};


NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Profile, name, access_path)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Settings, exec_mods_folder_path, mods_storage_path)

namespace
{
	using Data = std::pair<Settings, std::vector<Profile>>;
	Data GlobalData = {};
	bool LeaveProgram;


	namespace Utils
	{

		Settings CreateSettings()
		{
			std::cout << "Please select your Baldur's Gate 3 mods folder \n";
			std::ostringstream oss;
			oss << GetCustomPath(FOLDERID_LocalAppData) << "\\Larian Studios\\Baldur's Gate 3\\Mods";
			std::string mods_folder = SelectFolder(oss.str().c_str());


			std::cout << "Please select your profiles storage folder\n";
			oss = {};
			std::string mods_storage = SelectFolder("C:\\");

			return Settings{ .exec_mods_folder_path = mods_folder,  .mods_storage_path = mods_storage };
		}

		void CreateDefaultProfile()
		{
			json parser;

			parser[SettingsHolderName] = CreateSettings();

			parser[ProfilesHolderName] = json::array();

			std::ofstream file(SettingFileName);
			file << parser.dump(Indent);
			file.close();

			std::ostringstream oss;
			oss << "Default " << SettingFileName << " Created with success !\n";
			std::cout << oss.str();
		}

		void CheckAndLoadProfile()
		{
			if (!fs::exists(SettingFileName))
			{
				std::ostringstream oss;
				oss << "Cannot find " << SettingFileName << ", creating default file\n";
				std::cout << oss.str();
				CreateDefaultProfile();
				std::cout << "\n\n";
			}

			json parser;
			Settings settings;
			std::vector<Profile> profiles;
			std::ifstream file(SettingFileName);
			file >> parser;
			try
			{
				profiles = parser.at(ProfilesHolderName).get<std::vector<Profile>>();
				settings = parser.at(SettingsHolderName).get<Settings>();
			}
			catch (const json::exception& e)
			{
				std::cerr << "Erreur: " << e.what() << std::endl;
			}

			file.close();
			GlobalData = { settings,profiles };
		}

		void DisplayProfiles()
		{
			std::ostringstream oss;
			oss << GlobalData.second.size() << " profile found :\n"
				<< "\t0 - Go back to menu\n";
			std::cout << oss.str();

			int index = 1;
			for (auto profile : GlobalData.second)
			{
				oss = {};
				oss << "\t" << index++ << " - " << profile.name << "\n";
				std::cout << oss.str();
			}
		}


		Profile ChooseProfile()
		{
			DisplayProfiles();
			int choice = GetSecureNumericInput(0, static_cast<int>(GlobalData.second.size() + 1), "Choose a profile by his number : ") - 1;
			if (choice == -1)
			{
				return {};
			}
			return GlobalData.second[choice];
		}


		void AddNewProfile(const Profile& new_profile)
		{
			std::ifstream ifile(SettingFileName);
			json parser;
			ifile >> parser;
			ifile.close();

			parser[ProfilesHolderName].push_back(new_profile);


			std::ofstream ofile(SettingFileName);
			ofile << parser.dump(Indent);
			ofile.close();

			CheckAndLoadProfile();
		}

		void RemoveProfile(const Profile& profile_to_delete)
		{
			std::ifstream ifile(SettingFileName);
			json parser;
			ifile >> parser;
			ifile.close();


			int index = 0;
			for (int i = 0; i < parser[ProfilesHolderName].size(); i++)
			{
				if (parser[ProfilesHolderName][i].get<Profile>().name == profile_to_delete.name)
				{
					index = i;
					break;
				}
			}

			parser[ProfilesHolderName].erase(index);


			std::ofstream ofile(SettingFileName);
			ofile << parser.dump(Indent);
			ofile.close();

			CheckAndLoadProfile();
		}


		void CreateProfileDirectory(const Profile& profile)
		{
			fs::create_directories(profile.access_path);
			fs::create_directories(profile.access_path + "\\Mods");
		}

		void CopyCurrentMods(const Profile& profile)
		{
			if (profile.name == InvalidProfileName)
			{
				return;
			}

			fs::copy(GlobalData.first.exec_mods_folder_path, profile.access_path + "\\Mods", fs::copy_options::recursive | fs::copy_options::overwrite_existing);
			fs::copy(GlobalData.first.exec_mods_folder_path + "\\..\\" + ModsListSettingsPath, profile.access_path, fs::copy_options::recursive | fs::copy_options::recursive | fs::copy_options::overwrite_existing);
		}

	}

	namespace Commands
	{
		void Leave();

		void SelectProfileAndLaunch()
		{
			Profile profile = Utils::ChooseProfile();
			if (profile.name == InvalidProfileName)
			{
				return;
			}

			std::cout << "Loading " << profile.name << " profile...\n";

			fs::remove_all(GlobalData.first.exec_mods_folder_path);

			fs::copy(profile.access_path + "\\Mods", GlobalData.first.exec_mods_folder_path, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
			fs::copy(profile.access_path + "\\" + ModListFilename, GlobalData.first.exec_mods_folder_path + "\\..\\" + ModsListSettingsPath, fs::copy_options::recursive | fs::copy_options::overwrite_existing);

			system("start steam://rungameid/1086940");

			std::cout << profile.name << " profile is now loaded ! Enjoy your game !\n";

			Leave();
		}

		Profile CreateNewProfile()
		{
			std::string profile_name = getSecureStringInput(1, 25, false, "Enter a profile name (0 to go back to menu) : ");
			if (profile_name == InvalidProfileName)
			{
				return {};
			}

			std::ostringstream oss;
			oss << GlobalData.first.mods_storage_path << "\\" << profile_name;
			if (fs::exists(oss.str()))
			{
				std::cout << "This profile already exist !\n";
				return CreateNewProfile();
			}

			Profile new_profile{ .name = profile_name, .access_path = oss.str() };

			Utils::CreateProfileDirectory(new_profile);

			std::cout << "New profile " << profile_name << " created with success !\n";

			Utils::AddNewProfile(new_profile);

			return new_profile;
		}

		void CreateNewProfileFromCurrentMods()
		{
			Profile new_profile = CreateNewProfile();
			if (new_profile.name == InvalidProfileName)
			{
				return;
			}

			if (!fs::exists(GlobalData.first.exec_mods_folder_path))
			{
				std::cout << "Baldur's Gate 3 mods folder doesn't exist ! Setup your settings first.\n";
				return;
			}
			if (!fs::exists(new_profile.access_path) || !fs::exists(new_profile.access_path + "\\Mods"))
			{
				std::cout << "this profile doesn't have a folder yet ! Creating a new one.\n";
				Utils::CreateProfileDirectory(new_profile);
			}

			Utils::CopyCurrentMods(new_profile);

			std::cout << "Mods copied from the current mods folder with success !\n";
		}




		void UpdateExistingProfileFromCurrentMods()
		{
			Profile profile = Utils::ChooseProfile();

			Utils::CopyCurrentMods(profile);
			std::cout << "Mods copied from the current mods folder with success !\n";
		}


		void DeleteProfile()
		{
			Profile profile = Utils::ChooseProfile();
			if (profile.name == InvalidProfileName)
			{
				return;
			}

			std::cout << "Starting delete of " + profile.name << "\n";

			if (fs::exists(profile.access_path))
			{
				fs::remove_all(profile.access_path);
				fs::remove(profile.access_path);
			}
			Utils::RemoveProfile(profile);

			std::cout << "Profile " << profile.name << " deleted with success :\n";
		}

		void SetupSettings()
		{
			std::ifstream ifile(SettingFileName);
			json parser;
			ifile >> parser;
			ifile.close();

			parser[SettingsHolderName] = Utils::CreateSettings();


			std::ofstream ofile(SettingFileName);
			ofile << parser.dump(Indent);
			ofile.close();

			Utils::CheckAndLoadProfile();
		}

		void Leave()
		{
			LeaveProgram = true;
		}
	}


	void MainLoop()
	{
		Utils::CheckAndLoadProfile();
		int choice = -1;
		while (!LeaveProgram)
		{
			std::cout << "1 - Select a Profile and launch the game\n"
				<< "2 - Create a new empty Profile\n"
				<< "3 - Create a new Profile from current mods folder\n"
				<< "4 - Update Profile from the current mods folder\n"
				<< "5 - Delete a Profile\n"
				<< "6 - Setup Settings\n"
				<< "0 - Leave\n";
			choice = GetSecureNumericInput(0, 5);
			std::system("CLS");

			switch (choice)
			{
			case 0:
				Commands::Leave();
				break;
			case 1:
				Commands::SelectProfileAndLaunch();
				break;
			case 2:
				Commands::CreateNewProfile();
				break;
			case 3:
				Commands::CreateNewProfileFromCurrentMods();
				break;
			case 4:
				Commands::UpdateExistingProfileFromCurrentMods();
				break;
			case 5:
				Commands::DeleteProfile();
				break;
			case 6:
				Commands::SetupSettings();
				break;
			default:
				break;
			}

			std::cout << "\n\n";
		}

	}




}



int main()
{
	MainLoop();
}