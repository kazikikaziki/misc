#pragma once

namespace mo {
class Path;
class FileInput;
class FileOutput;

class KPacFileWriterImpl; // Internal class
class KPacFileReaderImpl; // Internal class

class KPacFileWriter {
public:
	KPacFileWriter();
	KPacFileWriter(const Path &filename);
	KPacFileWriter(FileOutput &file);
	bool open(const Path &filename);
	bool open(FileOutput &file);
	void close();
	bool isOpen() const;
	bool addEntryFromFileName(const Path &entry_name, const Path &filename);
	bool addEntryFromMemory(const Path &entry_name, const void *data, size_t size);

private:
	std::shared_ptr<KPacFileWriterImpl> impl_;
};

class KPacFileReader {
public:
	KPacFileReader();
	KPacFileReader(const Path &filename);
	KPacFileReader(FileInput &file);
	bool open(const Path &filename);
	bool open(FileInput &file);
	void close();
	bool isOpen() const;
	int getCount() const;
	int getIndexByName(const Path &entry_name, bool ignore_case, bool ignore_path) const;
	Path getName(int index) const;
	std::string getData(int index) const;

private:
	mutable std::shared_ptr<KPacFileReaderImpl> impl_;
};

}
