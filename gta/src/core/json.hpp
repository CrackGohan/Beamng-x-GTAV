// Minimal JSON for the bridge: a streaming writer and a small DOM parser. Only what the generated
// protocol code (gen/protocol.hpp) needs.
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace beamls::json
{
	class Value
	{
	public:
		enum class Type { Null, Bool, Number, String, Array, Object };

		Type type = Type::Null;
		bool b = false;
		double n = 0.0;
		std::string s;
		std::vector<Value> items;                          // Array
		std::vector<std::pair<std::string, Value>> fields; // Object

		bool isObject() const { return type == Type::Object; }
		bool isArray() const { return type == Type::Array; }
		const Value *get(const char *key) const;
		double num() const { return type == Type::Number ? n : (type == Type::Bool ? (b ? 1.0 : 0.0) : 0.0); }
		bool boolean() const { return type == Type::Bool ? b : (type == Type::Number ? n != 0.0 : false); }
		const std::string &str() const { return s; }
		const std::vector<Value> &arr() const { return items; }
	};

	// Parse text; returns false on malformed input (out is then Null).
	bool parse(const char *text, std::size_t len, Value &out);

	class Writer
	{
	public:
		void begin();
		void end();
		void str(const char *key, const std::string &v);
		void num(const char *key, double v);
		void integer(const char *key, long long v);
		void boolean(const char *key, bool v);
		void floats(const char *key, const std::vector<float> &v);
		void beginArray(const char *key);
		void endArray();
		void beginRow();
		void endRow();
		void rowNum(double v);
		void rowInt(long long v);
		std::string take() { return std::move(out_); }

	private:
		void key(const char *k);
		void sep();
		void number(double v);
		std::string out_;
		bool first_ = true;
		bool rowFirst_ = true;
		bool arrayFirst_ = true;
	};
}
