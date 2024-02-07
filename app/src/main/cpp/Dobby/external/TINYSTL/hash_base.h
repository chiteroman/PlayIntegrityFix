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

#ifndef TINYSTL_HASH_BASE_H
#define TINYSTL_HASH_BASE_H

#include <TINYSTL/stddef.h>
#include <TINYSTL/traits.h>

namespace tinystl {

	template<typename Key, typename Value>
	struct pair {
		pair();
		pair(const pair& other);
		pair(pair&& other);
		pair(const Key& key, const Value& value);
		pair(Key&& key, Value&& value);

		pair& operator=(const pair& other);
		pair& operator=(pair&& other);

		Key first;
		Value second;
	};

	template<typename Key, typename Value>
	inline pair<Key, Value>::pair() {
	}

	template<typename Key, typename Value>
	inline pair<Key, Value>::pair(const pair& other)
		: first(other.first)
		, second(other.second)
	{
	}

	template<typename Key, typename Value>
	inline pair<Key, Value>::pair(pair&& other)
		: first(static_cast<Key&&>(other.first))
		, second(static_cast<Value&&>(other.second))
	{
	}

	template<typename Key, typename Value>
	inline pair<Key, Value>::pair(const Key& key, const Value& value)
		: first(key)
		, second(value)
	{
	}

	template<typename Key, typename Value>
	inline pair<Key, Value>::pair(Key&& key, Value&& value)
		: first(static_cast<Key&&>(key))
		, second(static_cast<Value&&>(value))
	{
	}

	template<typename Key, typename Value>
	inline pair<Key, Value>& pair<Key, Value>::operator=(const pair& other) {
		first = other.first;
		second = other.second;
		return *this;
	}

	template<typename Key, typename Value>
	inline pair<Key, Value>& pair<Key, Value>::operator=(pair&& other) {
		first = static_cast<Key&&>(other.first);
		second = static_cast<Value&&>(other.second);
		return *this;
	}

	template<typename Key, typename Value>
	static inline pair<typename remove_reference<Key>::type, typename remove_reference<Value>::type>
	make_pair(Key&& key, Value&& value) {
		return pair<typename remove_reference<Key>::type, typename remove_reference<Value>::type>(
				  static_cast<Key&&>(key)
				, static_cast<Value&&>(value)
			);
	}


	template<typename Key, typename Value>
	struct unordered_hash_node {
		unordered_hash_node(const Key& key, const Value& value);
		unordered_hash_node(Key&& key, Value&& value);

		const Key first;
		Value second;
		unordered_hash_node* next;
		unordered_hash_node* prev;

	private:
		unordered_hash_node& operator=(const unordered_hash_node&);
	};

	template<typename Key, typename Value>
	inline unordered_hash_node<Key, Value>::unordered_hash_node(const Key& key, const Value& value)
		: first(key)
		, second(value)
	{
	}

	template<typename Key, typename Value>
	inline unordered_hash_node<Key, Value>::unordered_hash_node(Key&& key, Value&& value)
		: first(static_cast<Key&&>(key))
		, second(static_cast<Value&&>(value))
	{
	}

	template <typename Key>
	struct unordered_hash_node<Key, void> {
		explicit unordered_hash_node(const Key& key);
		explicit unordered_hash_node(Key&& key);

		const Key first;
		unordered_hash_node* next;
		unordered_hash_node* prev;

	private:
		unordered_hash_node& operator=(const unordered_hash_node&);
	};

	template<typename Key>
	inline unordered_hash_node<Key, void>::unordered_hash_node(const Key& key)
		: first(key)
	{
	}

	template<typename Key>
	inline unordered_hash_node<Key, void>::unordered_hash_node(Key&& key)
		: first(static_cast<Key&&>(key))
	{
	}

	template<typename Key, typename Value>
	static inline void unordered_hash_node_insert(unordered_hash_node<Key, Value>* node, size_t hash, unordered_hash_node<Key, Value>** buckets, size_t nbuckets) {
		size_t bucket = hash & (nbuckets - 1);

		unordered_hash_node<Key, Value>* it = buckets[bucket + 1];
		node->next = it;
		if (it) {
			node->prev = it->prev;
			it->prev = node;
			if (node->prev)
				node->prev->next = node;
		} else {
			size_t newbucket = bucket;
			while (newbucket && !buckets[newbucket])
				--newbucket;

			unordered_hash_node<Key, Value>* prev = buckets[newbucket];
			while (prev && prev->next)
				prev = prev->next;

			node->prev = prev;
			if (prev)
				prev->next = node;
		}

		// propagate node through buckets
		for (; it == buckets[bucket]; --bucket) {
			buckets[bucket] = node;
			if (!bucket)
				break;
		}
	}

	template<typename Key, typename Value>
	static inline void unordered_hash_node_erase(const unordered_hash_node<Key, Value>* where, size_t hash, unordered_hash_node<Key, Value>** buckets, size_t nbuckets) {
		size_t bucket = hash & (nbuckets - 1);

		unordered_hash_node<Key, Value>* next = where->next;
		for (; buckets[bucket] == where; --bucket) {
			buckets[bucket] = next;
			if (!bucket)
				break;
		}

		if (where->prev)
			where->prev->next = where->next;
		if (next)
			next->prev = where->prev;
	}

	template<typename Node>
	struct unordered_hash_iterator {
		Node* operator->() const;
		Node& operator*() const;
		Node* node;
	};

	template<typename Node>
	struct unordered_hash_iterator<const Node> {

		unordered_hash_iterator() {}
		unordered_hash_iterator(unordered_hash_iterator<Node> other)
			: node(other.node)
		{
		}

		const Node* operator->() const;
		const Node& operator*() const;
		const Node* node;
	};

	template<typename Key>
	struct unordered_hash_iterator<const unordered_hash_node<Key, void> > {
		const Key* operator->() const;
		const Key& operator*() const;
		unordered_hash_node<Key, void>* node;
	};

	template<typename LNode, typename RNode>
	static inline bool operator==(const unordered_hash_iterator<LNode>& lhs, const unordered_hash_iterator<RNode>& rhs) {
		return lhs.node == rhs.node;
	}

	template<typename LNode, typename RNode>
	static inline bool operator!=(const unordered_hash_iterator<LNode>& lhs, const unordered_hash_iterator<RNode>& rhs) {
		return lhs.node != rhs.node;
	}

	template<typename Node>
	static inline void operator++(unordered_hash_iterator<Node>& lhs) {
		lhs.node = lhs.node->next;
	}

	template<typename Node>
	inline Node* unordered_hash_iterator<Node>::operator->() const {
		return node;
	}

	template<typename Node>
	inline Node& unordered_hash_iterator<Node>::operator*() const {
		return *node;
	}

	template<typename Node>
	inline const Node* unordered_hash_iterator<const Node>::operator->() const {
		return node;
	}

	template<typename Node>
	inline const Node& unordered_hash_iterator<const Node>::operator*() const {
		return *node;
	}

	template<typename Key>
	inline const Key* unordered_hash_iterator<const unordered_hash_node<Key, void> >::operator->() const {
		return &node->first;
	}

	template<typename Key>
	inline const Key& unordered_hash_iterator<const unordered_hash_node<Key, void> >::operator*() const {
		return node->first;
	}

	template<typename Node, typename Key>
	static inline Node unordered_hash_find(const Key& key, Node* buckets, size_t nbuckets) {
		const size_t bucket = hash(key) & (nbuckets - 2);
		for (Node it = buckets[bucket], end = buckets[bucket+1]; it != end; it = it->next)
			if (it->first == key)
				return it;

		return 0;
	}
}
#endif
