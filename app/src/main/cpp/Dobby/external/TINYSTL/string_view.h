/*-
 * Copyright 2012-1017 Matthew Endsley
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TINYSTL_STRING_VIEW_H
#define TINYSTL_STRING_VIEW_H

#include <TINYSTL/stddef.h>

namespace tinystl {

	class string_view
	{
	public:
		typedef char value_type;
		typedef char* pointer;
		typedef const char* const_pointer;
		typedef char& reference;
		typedef const char& const_reference;
		typedef const_pointer iterator;
		typedef const_pointer const_iterator;
		typedef size_t size_type;
		typedef ptrdiff_t difference_type;

		static constexpr size_type npos = size_type(-1);

		constexpr string_view();
		constexpr string_view(const char* s, size_type count);
		constexpr string_view(const char* s);
		constexpr string_view(const string_view&) = default;
		string_view& operator=(const string_view&) = default;

		constexpr const char* data() const;
		constexpr char operator[](size_type pos) const;
		constexpr size_type size() const;
		constexpr bool empty() const;
		constexpr iterator begin() const;
		constexpr const_iterator cbegin() const;
		constexpr iterator end() const;
		constexpr const_iterator cend() const;
		constexpr string_view substr(size_type pos = 0, size_type count = npos) const;
		constexpr void swap(string_view& v);

	private:
		string_view(decltype(nullptr)) = delete;

		static constexpr size_type strlen(const char*);

		const char* m_str;
		size_type m_size;
	};

	constexpr string_view::string_view()
		: m_str(nullptr)
		, m_size(0)
	{
	}

	constexpr string_view::string_view(const char* s, size_type count)
		: m_str(s)
		, m_size(count)
	{
	}

	constexpr string_view::string_view(const char* s)
		: m_str(s)
		, m_size(strlen(s))
	{
	}

	constexpr const char* string_view::data() const {
		return m_str;
	}

	constexpr char string_view::operator[](size_type pos) const {
		return m_str[pos];
	}

	constexpr string_view::size_type string_view::size() const {
		return m_size;
	}

	constexpr bool string_view::empty() const {
    	return 0 == m_size;
	}

	constexpr string_view::iterator string_view::begin() const {
		return m_str;
	}

	constexpr string_view::const_iterator string_view::cbegin() const {
		return m_str;
	}

	constexpr string_view::iterator string_view::end() const {
		return m_str + m_size;
	}

	constexpr string_view::const_iterator string_view::cend() const {
		return m_str + m_size;
	}

	constexpr string_view string_view::substr(size_type pos, size_type count) const {
		return string_view(m_str + pos, npos == count ? m_size - pos : count);
	}

	constexpr void string_view::swap(string_view& v) {
		const char* strtmp = m_str;
		size_type sizetmp = m_size;
		m_str = v.m_str;
		m_size = v.m_size;
		v.m_str = strtmp;
		v.m_size = sizetmp;
	}

	constexpr string_view::size_type string_view::strlen(const char* s) {
		for (size_t len = 0; ; ++len) {
			if (0 == s[len]) {
				return len;
			}
		}
	}
}

#endif // TINYSTL_STRING_VIEW_H
