/*-
 * Copyright 2012-2018 Matthew Endsley
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

#ifndef TINYSTL_STRING_H
#define TINYSTL_STRING_H

#include <TINYSTL/allocator.h>
#include <TINYSTL/stddef.h>
#include <TINYSTL/hash.h>

namespace tinystl {

	template<typename Allocator>
	class basic_string {
	public:
		basic_string();
		basic_string(const basic_string& other);
		basic_string(basic_string&& other);
		basic_string(const char* sz);
		basic_string(const char* sz, size_t len);
		~basic_string();

		basic_string& operator=(const basic_string& other);
		basic_string& operator=(basic_string&& other);

		const char* c_str() const;
		size_t size() const;

		void reserve(size_t size);
		void resize(size_t size);

		void clear();
		void append(const char* first, const char* last);
		void assign(const char* s, size_t n);

		void shrink_to_fit();
		void swap(basic_string& other);

	private:
		typedef char* pointer;
		pointer m_first;
		pointer m_last;
		pointer m_capacity;

		static const size_t c_nbuffer = 12;
		char m_buffer[12];
	};

	template<typename allocator>
	inline basic_string<allocator>::basic_string()
		: m_first(m_buffer)
		, m_last(m_buffer)
		, m_capacity(m_buffer + c_nbuffer)
	{
		resize(0);
	}

	template<typename allocator>
	inline basic_string<allocator>::basic_string(const basic_string& other)
		: m_first(m_buffer)
		, m_last(m_buffer)
		, m_capacity(m_buffer + c_nbuffer)
	{
		reserve(other.size());
		append(other.m_first, other.m_last);
	}

	template<typename allocator>
	inline basic_string<allocator>::basic_string(basic_string&& other)
	{
		if (other.m_first == other.m_buffer) {
			m_first = m_buffer;
			m_last = m_buffer;
			m_capacity = m_buffer + c_nbuffer;
			reserve(other.size());
			append(other.m_first, other.m_last);
		} else {
			m_first = other.m_first;
			m_last = other.m_last;
			m_capacity = other.m_capacity;
		}
		other.m_first = other.m_last = other.m_buffer;
		other.m_capacity = other.m_buffer + c_nbuffer;
		other.resize(0);
	}

	template<typename allocator>
	inline basic_string<allocator>::basic_string(const char* sz)
		: m_first(m_buffer)
		, m_last(m_buffer)
		, m_capacity(m_buffer + c_nbuffer)
	{
		size_t len = 0;
		for (const char* it = sz; *it; ++it)
			++len;

		reserve(len);
		append(sz, sz + len);
	}

	template<typename allocator>
	inline basic_string<allocator>::basic_string(const char* sz, size_t len)
		: m_first(m_buffer)
		, m_last(m_buffer)
		, m_capacity(m_buffer + c_nbuffer)
	{
		reserve(len);
		append(sz, sz + len);
	}

	template<typename allocator>
	inline basic_string<allocator>::~basic_string() {
		if (m_first != m_buffer)
			allocator::static_deallocate(m_first, m_capacity - m_first);
	}

	template<typename allocator>
	inline basic_string<allocator>& basic_string<allocator>::operator=(const basic_string& other) {
		basic_string(other).swap(*this);
		return *this;
	}

	template<typename allocator>
	inline basic_string<allocator>& basic_string<allocator>::operator=(basic_string&& other) {
		basic_string(static_cast<basic_string&&>(other)).swap(*this);
		return *this;
	}

	template<typename allocator>
	inline const char* basic_string<allocator>::c_str() const {
		return m_first;
	}

	template<typename allocator>
	inline size_t basic_string<allocator>::size() const
	{
		return (size_t)(m_last - m_first);
	}

	template<typename allocator>
	inline void basic_string<allocator>::reserve(size_t capacity) {
		if (m_first + capacity + 1 <= m_capacity)
			return;

		const size_t size = (size_t)(m_last - m_first);

		pointer newfirst = (pointer)allocator::static_allocate(capacity + 1);
		for (pointer it = m_first, newit = newfirst, end = m_last; it != end; ++it, ++newit)
			*newit = *it;
		if (m_first != m_buffer)
			allocator::static_deallocate(m_first, m_capacity - m_first);

		m_first = newfirst;
		m_last = newfirst + size;
		m_capacity = m_first + capacity;
	}

	template<typename allocator>
	inline void basic_string<allocator>::resize(size_t size) {
		const size_t prevSize = m_last-m_first;
		reserve(size);
		if (size > prevSize)
			for (pointer it = m_last, end = m_first + size + 1; it < end; ++it)
				*it = 0;
		else if (m_last != m_first)
			m_first[size] = 0;

		m_last = m_first + size;
	}

	template<typename allocator>
	inline void basic_string<allocator>::clear() {
		resize(0);
	}

	template<typename allocator>
	inline void basic_string<allocator>::append(const char* first, const char* last) {
		const size_t newsize = (size_t)((m_last - m_first) + (last - first) + 1);
		if (m_first + newsize > m_capacity)
			reserve((newsize * 3) / 2);

		for (; first != last; ++m_last, ++first)
			*m_last = *first;
		*m_last = 0;
	}

	template<typename allocator>
	inline void basic_string<allocator>::assign(const char* sz, size_t n) {
		clear();
		append(sz, sz+n);
	}

	template<typename allocator>
	inline void basic_string<allocator>::shrink_to_fit() {
		if (m_first == m_buffer) {
		} else if (m_last == m_first) {
			const size_t capacity = (size_t)(m_capacity - m_first);
			if (capacity)
				allocator::static_deallocate(m_first, capacity+1);
			m_capacity = m_first;
		} else if (m_capacity != m_last) {
			const size_t size = (size_t)(m_last - m_first);
			char* newfirst = (pointer)allocator::static_allocate(size+1);
			for (pointer in = m_first, out = newfirst; in != m_last + 1; ++in, ++out)
				*out = *in;
			if (m_first != m_capacity)
				allocator::static_deallocate(m_first, m_capacity+1-m_first);
			m_first = newfirst;
			m_last = newfirst+size;
			m_capacity = m_last;
		}
	}

	template<typename allocator>
	inline void basic_string<allocator>::swap(basic_string& other) {
		const pointer tfirst = m_first, tlast = m_last, tcapacity = m_capacity;
		m_first = other.m_first, m_last = other.m_last, m_capacity = other.m_capacity;
		other.m_first = tfirst, other.m_last = tlast, other.m_capacity = tcapacity;

		char tbuffer[c_nbuffer];

		if (m_first == other.m_buffer)
			for  (pointer it = other.m_buffer, end = m_last, out = tbuffer; it != end; ++it, ++out)
				*out = *it;

		if (other.m_first == m_buffer) {
			other.m_last = other.m_last - other.m_first + other.m_buffer;
			other.m_first = other.m_buffer;
			other.m_capacity = other.m_buffer + c_nbuffer;

			for (pointer it = other.m_first, end = other.m_last, in = m_buffer; it != end; ++it, ++in)
				*it = *in;
			*other.m_last = 0;
		}

		if (m_first == other.m_buffer) {
			m_last = m_last - m_first + m_buffer;
			m_first = m_buffer;
			m_capacity = m_buffer + c_nbuffer;

			for (pointer it = m_first, end = m_last, in = tbuffer; it != end; ++it, ++in)
				*it = *in;
			*m_last = 0;
		}
	}

	template<typename allocatorl, typename allocatorr>
	inline bool operator==(const basic_string<allocatorl>& lhs, const basic_string<allocatorr>& rhs) {
		typedef const char* pointer;

		const size_t lsize = lhs.size(), rsize = rhs.size();
		if (lsize != rsize)
			return false;

		pointer lit = lhs.c_str(), rit = rhs.c_str();
		pointer lend = lit + lsize;
		while (lit != lend)
			if (*lit++ != *rit++)
				return false;

		return true;
	}

	template<typename allocator>
	static inline size_t hash(const basic_string<allocator>& value) {
		return hash_string(value.c_str(), value.size());
	}

	typedef basic_string<TINYSTL_ALLOCATOR> string;
}

#endif
