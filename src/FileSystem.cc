#include <ios>
#include <boost/filesystem/fstream.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/algorithm/string.hpp>
#include "FileSystem.hpp"
#include "Util.hpp"

namespace Sit {
namespace FileSystem {

const boost::filesystem::path SIT_ROOT(".sit/");
const boost::filesystem::path OBJECTS_DIR(".sit/objects");

boost::filesystem::path REPO_ROOT("./");

bool InRepo()
{
	return boost::filesystem::is_directory(REPO_ROOT / SIT_ROOT);
}

void AssertInRepo()
{
	if (!InRepo()) {
		throw Sit::Util::SitException("Fatal: Not in a sit repo.");
	}
}

std::vector<boost::filesystem::path> ListRecursive(const boost::filesystem::path &path, const bool ignoreSit, const bool relativeToRepoRoot)
{
	using namespace boost::filesystem;
	std::vector<boost::filesystem::path> ls;

	auto insert_list = [&] (const boost::filesystem::path & path) {
		const boost::filesystem::path p(GetRelativePath(path));
		const std::string s(p.generic_string());
		if (relativeToRepoRoot) {
			ls.push_back(p);
		} else {
			ls.push_back(path);
		}
	};

	try {
		if (IsDirectory(path)) {
			for(directory_iterator iter(path), end; iter != end ; ++iter) {
				const auto fn = iter->path().filename();
				if (fn == ".sit" && ignoreSit) continue;

				if (is_regular_file(iter->status())) {
					insert_list(iter->path());
					continue;
				}

				for (recursive_directory_iterator iter(path / fn), end; iter != end; ++iter) {
					insert_list(iter->path());
				}
			}
		} else {
			ls.push_back(GetRelativePath(path));
		}
	} catch (const filesystem_error &fe) {
		throw Sit::Util::SitException(std::string("Fatal: Cannot list path: ") + path.string(), fe.what());
	}
	return ls;
}

void SafeCopyFile(const boost::filesystem::path &from, const boost::filesystem::path &to)
{
	using namespace boost::filesystem;
	if (IsDirectory(from)) {
		return;
	}
	create_directories(to.parent_path());
	copy_file(from, to, copy_option::overwrite_if_exists);
}

bool IsExist(const boost::filesystem::path &path)
{
	return boost::filesystem::exists(path);
}

bool IsFile(const boost::filesystem::path &path)
{
	return boost::filesystem::is_regular_file(path);
}

bool IsDirectory(const boost::filesystem::path &path)
{
	return boost::filesystem::is_directory(path);
}

void Write(const boost::filesystem::path &path, const std::string &content)
{
	using namespace boost::filesystem;
	create_directories(path.parent_path());
	ofstream file(path, std::ios::out | std::ios::binary);
	file.write(content.c_str(), content.length());
	if (!file) {
		throw Util::SitException(std::string("Cannot write to path: ") + path.string());
	}
}

std::string Read(const boost::filesystem::path &path)
{
	using namespace boost::iostreams;
	auto fileSize = boost::filesystem::file_size(path);
	if (fileSize > 512 << 20) {
		throw Sit::Util::SitException("Fatal: try to read a file larger than 512MB.");
	}
	char *s = new char[fileSize];
	boost::filesystem::ifstream file(path, std::ios::in | std::ios::binary);
	file.read(s, fileSize);
	std::string str(s, s + fileSize);
	delete[] s;
	return str;
}

void Remove(const boost::filesystem::path& path)
{
	boost::filesystem::remove(path);
}

boost::filesystem::path GetRelativePath(
	const boost::filesystem::path &file,
	const boost::filesystem::path &base)
{
	using namespace boost::filesystem;

	std::string strFullPath = absolute(file).generic_string();
	std::vector<std::string> tmpDirsFile, finalDirsFile;
	boost::split(tmpDirsFile, strFullPath, boost::is_any_of("/"));
	for (const auto &element : tmpDirsFile) {
		if (element == "" || element == ".") {
			continue;
		}
		if (element == "..") {
			if (finalDirsFile.empty()) {
				throw Sit::Util::SitException("Fatal: This is a illegal relative path.");
			} else {
				finalDirsFile.pop_back();
			}
			continue;
		}
		finalDirsFile.push_back(element);
	}

	std::string strFullPathBase = absolute(base).generic_string();
	std::vector<std::string> tmpDirsBase, finalDirsBase;
	boost::split(tmpDirsBase, strFullPathBase, boost::is_any_of("/"));
	for (const auto &element : tmpDirsBase) {
		if (element == "" || element == ".") {
			continue;
		}
		if (element == "..") {
			if (finalDirsBase.empty()) {
				throw Sit::Util::SitException("Fatal: This is a illegal relative path.");
			} else {
				finalDirsBase.pop_back();
			}
			continue;
		}
		finalDirsBase.push_back(element);
	}

	unsigned sameItemCount = 0;

	for (unsigned i = 0; i < std::min(finalDirsBase.size(), finalDirsFile.size()); ++i) {
		if (finalDirsBase[i] == finalDirsFile[i]) {
			++sameItemCount;
		} else {
			break;
		}
	}

	path resultPath("");
	for (unsigned i = sameItemCount; i < finalDirsBase.size(); ++i) {
		if (resultPath.empty()) {
			resultPath = "..";
		} else {
			resultPath = resultPath / "..";
		}
	}
	for (unsigned i = sameItemCount; i < finalDirsFile.size(); ++i) {
		if (resultPath.empty()) {
			resultPath = finalDirsFile[i];
		} else {
			resultPath = resultPath / finalDirsFile[i];
		}
	}

	return resultPath;
}

std::string FileSHA1(const boost::filesystem::path &path)
{
	static char buf[65536];
	boost::uuids::detail::sha1 shaEngine;
	boost::filesystem::ifstream file(path, std::ios::in | std::ios::binary);
	unsigned shaValue[5] = {0};
	while (file.good()) {
		file.read(buf, sizeof(buf));
		shaEngine.process_bytes(buf, file.gcount());
	}
	if (!file.eof()) {
		throw Util::SitException(std::string("SHA1sum fail: ") + path.string());
	}
	shaEngine.get_digest(shaValue);
	std::ostringstream result;
	for (auto &x : shaValue) {
		result << std::setw(8) << std::setfill('0') << std::hex << x;
	}
	return result.str();
}

}
}
