#include "json.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace beamls::json
{
	const Value *Value::get(const char *key) const
	{
		if (type != Type::Object)
			return nullptr;
		for (const auto &f : fields)
			if (f.first == key)
				return &f.second;
		return nullptr;
	}

	namespace
	{
		struct Parser
		{
			const char *p;
			const char *end;
			int depth = 0;

			void ws()
			{
				while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
					++p;
			}

			bool lit(const char *w)
			{
				const std::size_t n = std::strlen(w);
				if (std::size_t(end - p) < n || std::memcmp(p, w, n) != 0)
					return false;
				p += n;
				return true;
			}

			bool string(std::string &out)
			{
				if (p >= end || *p != '"')
					return false;
				++p;
				while (p < end && *p != '"')
				{
					if (*p == '\\')
					{
						if (++p >= end)
							return false;
						switch (*p)
						{
						case 'n': out += '\n'; break;
						case 't': out += '\t'; break;
						case 'r': out += '\r'; break;
						case 'b': out += '\b'; break;
						case 'f': out += '\f'; break;
						case 'u':
						{
							if (end - p < 5)
								return false;
							char hex[5] = {p[1], p[2], p[3], p[4], 0};
							const long c = std::strtol(hex, nullptr, 16);
							if (c < 0x80)
								out += char(c);
							else if (c < 0x800)
							{
								out += char(0xC0 | (c >> 6));
								out += char(0x80 | (c & 0x3F));
							}
							else
							{
								out += char(0xE0 | (c >> 12));
								out += char(0x80 | ((c >> 6) & 0x3F));
								out += char(0x80 | (c & 0x3F));
							}
							p += 4;
							break;
						}
						default: out += *p; break;
						}
						++p;
					}
					else
						out += *p++;
				}
				if (p >= end)
					return false;
				++p;
				return true;
			}

			bool value(Value &v)
			{
				if (++depth > 32)
					return false;
				ws();
				if (p >= end)
					return false;
				bool ok = true;
				switch (*p)
				{
				case '{':
				{
					v.type = Value::Type::Object;
					++p;
					ws();
					if (p < end && *p == '}')
					{
						++p;
						break;
					}
					for (;;)
					{
						ws();
						std::string k;
						if (!string(k))
							return false;
						ws();
						if (p >= end || *p != ':')
							return false;
						++p;
						v.fields.emplace_back(std::move(k), Value());
						if (!value(v.fields.back().second))
							return false;
						ws();
						if (p < end && *p == ',')
						{
							++p;
							continue;
						}
						if (p < end && *p == '}')
						{
							++p;
							break;
						}
						return false;
					}
					break;
				}
				case '[':
				{
					v.type = Value::Type::Array;
					++p;
					ws();
					if (p < end && *p == ']')
					{
						++p;
						break;
					}
					for (;;)
					{
						v.items.emplace_back();
						if (!value(v.items.back()))
							return false;
						ws();
						if (p < end && *p == ',')
						{
							++p;
							continue;
						}
						if (p < end && *p == ']')
						{
							++p;
							break;
						}
						return false;
					}
					break;
				}
				case '"':
					v.type = Value::Type::String;
					ok = string(v.s);
					break;
				case 't':
					v.type = Value::Type::Bool;
					v.b = true;
					ok = lit("true");
					break;
				case 'f':
					v.type = Value::Type::Bool;
					v.b = false;
					ok = lit("false");
					break;
				case 'n':
					v.type = Value::Type::Null;
					ok = lit("null");
					break;
				default:
				{
					char buf[64];
					std::size_t n = 0;
					while (p < end && n < sizeof(buf) - 1 && (std::strchr("+-0123456789.eE", *p) != nullptr))
						buf[n++] = *p++;
					buf[n] = 0;
					if (n == 0)
						return false;
					char *e = nullptr;
					v.type = Value::Type::Number;
					v.n = std::strtod(buf, &e);
					ok = e == buf + n;
					break;
				}
				}
				--depth;
				return ok;
			}
		};
	}

	bool parse(const char *text, std::size_t len, Value &out)
	{
		Parser ps{text, text + len};
		out = Value();
		if (!ps.value(out))
		{
			out = Value();
			return false;
		}
		ps.ws();
		if (ps.p != ps.end)
		{
			out = Value();
			return false;
		}
		return true;
	}

	void Writer::begin()
	{
		out_.clear();
		out_ += '{';
		first_ = true;
	}

	void Writer::end() { out_ += '}'; }

	void Writer::sep()
	{
		if (!first_)
			out_ += ',';
		first_ = false;
	}

	void Writer::key(const char *k)
	{
		sep();
		out_ += '"';
		out_ += k;
		out_ += "\":";
	}

	void Writer::number(double v)
	{
		if (!std::isfinite(v))
		{
			out_ += '0';
			return;
		}
		char buf[40];
		std::snprintf(buf, sizeof(buf), "%.7g", v);
		out_ += buf;
	}

	void Writer::str(const char *k, const std::string &v)
	{
		key(k);
		out_ += '"';
		for (char c : v)
		{
			switch (c)
			{
			case '"': out_ += "\\\""; break;
			case '\\': out_ += "\\\\"; break;
			case '\n': out_ += "\\n"; break;
			case '\r': out_ += "\\r"; break;
			case '\t': out_ += "\\t"; break;
			default:
				if (static_cast<unsigned char>(c) < 0x20)
				{
					char buf[8];
					std::snprintf(buf, sizeof(buf), "\\u%04x", c);
					out_ += buf;
				}
				else
					out_ += c;
			}
		}
		out_ += '"';
	}

	void Writer::num(const char *k, double v)
	{
		key(k);
		number(v);
	}

	void Writer::integer(const char *k, long long v)
	{
		key(k);
		out_ += std::to_string(v);
	}

	void Writer::boolean(const char *k, bool v)
	{
		key(k);
		out_ += v ? "true" : "false";
	}

	void Writer::floats(const char *k, const std::vector<float> &v)
	{
		key(k);
		out_ += '[';
		for (std::size_t i = 0; i < v.size(); ++i)
		{
			if (i)
				out_ += ',';
			number(v[i]);
		}
		out_ += ']';
	}

	void Writer::beginArray(const char *k)
	{
		key(k);
		out_ += '[';
		arrayFirst_ = true;
	}

	void Writer::endArray() { out_ += ']'; }

	void Writer::beginRow()
	{
		if (!arrayFirst_)
			out_ += ',';
		arrayFirst_ = false;
		out_ += '[';
		rowFirst_ = true;
	}

	void Writer::endRow() { out_ += ']'; }

	void Writer::rowNum(double v)
	{
		if (!rowFirst_)
			out_ += ',';
		rowFirst_ = false;
		number(v);
	}

	void Writer::rowInt(long long v)
	{
		if (!rowFirst_)
			out_ += ',';
		rowFirst_ = false;
		out_ += std::to_string(v);
	}
}
