/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Ronin @feature 31/10/2026 DX9: tintinhamans' (GeneralsGameCode, arctic/go/feat/rmlui), ported as he wrote it.
// FILE: Signal.h //////////////////////////////////////////////////////////////
// Tiny multicast signal for screen events (LAN/online lobby, welcome, login, ...): any number
// of listeners, each holding a SignalConnection that disconnects on destruction. C++98 so Core
// stays VC6-clean: fixed arities instead of variadics, and connect() takes any callable
// (function pointer, functor, or a lambda where the compiler has them).
//
// - emit() is re-entrancy safe: listeners connected during an emit are not called until the
//   next one, listeners disconnected during an emit (including the caller itself) are skipped.
// - Disconnected slots are swept lazily, when no emit is in flight.
// - A connection may outlive its signal (the slot just goes dead), so function-local static
//   signals are safe against static destruction order.
// - Single-threaded, like the screens it serves.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <vector>

namespace SignalDetail
{
	// One listener. Refcounted so a SignalConnection and its signal can be destroyed in either order.
	struct Node
	{
		Node() : refs( 1 ), alive( true ) {}
		virtual ~Node() {}
		void addRef() { ++refs; }
		void release() { if ( --refs == 0 ) delete this; }
		int refs;
		bool alive;
	};

	// Listener list shared by every arity.
	class Slots
	{
	public:
		Slots() : m_depth( 0 ) {}
		~Slots()
		{
			for ( size_t i = 0; i < m_nodes.size(); ++i )
			{
				m_nodes[i]->alive = false;
				m_nodes[i]->release();
			}
		}

		void add( Node *node )
		{
			if ( m_depth == 0 )
				sweep();
			node->addRef();
			m_nodes.push_back( node );
		}

		bool hasListeners() const
		{
			for ( size_t i = 0; i < m_nodes.size(); ++i )
			{
				if ( m_nodes[i]->alive )
					return true;
			}
			return false;
		}

		size_t size() const { return m_nodes.size(); }
		Node *at( size_t i ) const { return m_nodes[i]; }
		void beginEmit() { ++m_depth; }
		void endEmit()
		{
			if ( --m_depth == 0 )
				sweep();
		}

		// Brackets one emit so a listener that throws still unwinds the depth.
		class EmitScope
		{
		public:
			explicit EmitScope( Slots &slots ) : m_slots( slots ) { m_slots.beginEmit(); }
			~EmitScope() { m_slots.endEmit(); }
		private:
			EmitScope( const EmitScope & );
			EmitScope &operator=( const EmitScope & );
			Slots &m_slots;
		};

	private:
		Slots( const Slots & );
		Slots &operator=( const Slots & );

		void sweep()
		{
			size_t out = 0;
			for ( size_t i = 0; i < m_nodes.size(); ++i )
			{
				if ( m_nodes[i]->alive )
					m_nodes[out++] = m_nodes[i];
				else
					m_nodes[i]->release();
			}
			m_nodes.resize( out );
		}

		std::vector<Node *> m_nodes;
		int m_depth;
	};

	struct Node0 : Node { virtual void call() = 0; };
	template<class A> struct Node1 : Node { virtual void call( A a ) = 0; };
	template<class A, class B> struct Node2 : Node { virtual void call( A a, B b ) = 0; };
	template<class A, class B, class C> struct Node3 : Node { virtual void call( A a, B b, C c ) = 0; };
	struct NodeBool0 : Node { virtual bool call() = 0; };

	template<class F> struct Fn0 : Node0
	{
		explicit Fn0( F fn ) : f( fn ) {}
		virtual void call() { f(); }
		F f;
	};
	template<class A, class F> struct Fn1 : Node1<A>
	{
		explicit Fn1( F fn ) : f( fn ) {}
		virtual void call( A a ) { f( a ); }
		F f;
	};
	template<class A, class B, class F> struct Fn2 : Node2<A, B>
	{
		explicit Fn2( F fn ) : f( fn ) {}
		virtual void call( A a, B b ) { f( a, b ); }
		F f;
	};
	template<class A, class B, class C, class F> struct Fn3 : Node3<A, B, C>
	{
		explicit Fn3( F fn ) : f( fn ) {}
		virtual void call( A a, B b, C c ) { f( a, b, c ); }
		F f;
	};
	template<class F> struct FnBool0 : NodeBool0
	{
		explicit FnBool0( F fn ) : f( fn ) {}
		virtual bool call() { return f() ? true : false; }
		F f;
	};
}

// RAII handle for one listener: disconnects on destruction. Keep it for as long as the listener
// should stay connected (connect in show()/init, drop in hide()/shutdown). Move-only where the
// compiler has rvalue references; on C++98 a copy transfers ownership (auto_ptr style).
class SignalConnection
{
public:
	SignalConnection() : m_node( 0 ) {}
	explicit SignalConnection( SignalDetail::Node *node ) : m_node( node ) {} // adopts the caller's reference
	~SignalConnection() { disconnect(); }

#if defined(__cplusplus) && __cplusplus >= 201103L
	SignalConnection( SignalConnection &&other ) noexcept : m_node( other.m_node ) { other.m_node = 0; }
	SignalConnection &operator=( SignalConnection &&other ) noexcept
	{
		if ( this != &other )
		{
			disconnect();
			m_node = other.m_node;
			other.m_node = 0;
		}
		return *this;
	}
	SignalConnection( const SignalConnection & ) = delete;
	SignalConnection &operator=( const SignalConnection & ) = delete;
#else
	SignalConnection( const SignalConnection &other ) : m_node( other.m_node ) { other.m_node = 0; }
	SignalConnection &operator=( const SignalConnection &other )
	{
		if ( this != &other )
		{
			disconnect();
			m_node = other.m_node;
			other.m_node = 0;
		}
		return *this;
	}
#endif

	void disconnect()
	{
		if ( m_node != 0 )
		{
			m_node->alive = false;
			m_node->release();
			m_node = 0;
		}
	}

	bool connected() const { return m_node != 0 && m_node->alive; }

	// Gives up ownership without disconnecting (SignalConnections adopts it).
	SignalDetail::Node *detach()
	{
		SignalDetail::Node *node = m_node;
		m_node = 0;
		return node;
	}

private:
#if defined(__cplusplus) && __cplusplus >= 201103L
	SignalDetail::Node *m_node;
#else
	mutable SignalDetail::Node *m_node;
#endif
};

// A screen's whole set of listeners: add() every connect() result, disconnect() once in hide()/
// shutdown (also runs on destruction).
class SignalConnections
{
public:
	SignalConnections() {}
	~SignalConnections() { disconnect(); }

	void add( SignalConnection connection )
	{
		SignalDetail::Node *node = connection.detach();
		if ( node != 0 )
			m_nodes.push_back( node );
	}

	void disconnect()
	{
		for ( size_t i = 0; i < m_nodes.size(); ++i )
		{
			m_nodes[i]->alive = false;
			m_nodes[i]->release();
		}
		m_nodes.clear();
	}

private:
	SignalConnections( const SignalConnections & );
	SignalConnections &operator=( const SignalConnections & );

	std::vector<SignalDetail::Node *> m_nodes;
};

class Signal0
{
public:
	typedef SignalConnection Connection;

	template<class F> Connection connect( F fn )
	{
		SignalDetail::Fn0<F> *node = new SignalDetail::Fn0<F>( fn );
		m_slots.add( node );
		return Connection( node );
	}

	void emit()
	{
		const size_t count = m_slots.size();
		SignalDetail::Slots::EmitScope scope( m_slots );
		for ( size_t i = 0; i < count; ++i )
		{
			SignalDetail::Node0 *node = static_cast<SignalDetail::Node0 *>( m_slots.at( i ) );
			if ( node->alive )
				node->call();
		}
	}

	bool hasListeners() const { return m_slots.hasListeners(); }

private:
	SignalDetail::Slots m_slots;
};

template<class A>
class Signal1
{
public:
	typedef SignalConnection Connection;

	template<class F> Connection connect( F fn )
	{
		SignalDetail::Fn1<A, F> *node = new SignalDetail::Fn1<A, F>( fn );
		m_slots.add( node );
		return Connection( node );
	}

	void emit( A a )
	{
		const size_t count = m_slots.size();
		SignalDetail::Slots::EmitScope scope( m_slots );
		for ( size_t i = 0; i < count; ++i )
		{
			SignalDetail::Node1<A> *node = static_cast<SignalDetail::Node1<A> *>( m_slots.at( i ) );
			if ( node->alive )
				node->call( a );
		}
	}

	bool hasListeners() const { return m_slots.hasListeners(); }

private:
	SignalDetail::Slots m_slots;
};

template<class A, class B>
class Signal2
{
public:
	typedef SignalConnection Connection;

	template<class F> Connection connect( F fn )
	{
		SignalDetail::Fn2<A, B, F> *node = new SignalDetail::Fn2<A, B, F>( fn );
		m_slots.add( node );
		return Connection( node );
	}

	void emit( A a, B b )
	{
		const size_t count = m_slots.size();
		SignalDetail::Slots::EmitScope scope( m_slots );
		for ( size_t i = 0; i < count; ++i )
		{
			SignalDetail::Node2<A, B> *node = static_cast<SignalDetail::Node2<A, B> *>( m_slots.at( i ) );
			if ( node->alive )
				node->call( a, b );
		}
	}

	bool hasListeners() const { return m_slots.hasListeners(); }

private:
	SignalDetail::Slots m_slots;
};

template<class A, class B, class C>
class Signal3
{
public:
	typedef SignalConnection Connection;

	template<class F> Connection connect( F fn )
	{
		SignalDetail::Fn3<A, B, C, F> *node = new SignalDetail::Fn3<A, B, C, F>( fn );
		m_slots.add( node );
		return Connection( node );
	}

	void emit( A a, B b, C c )
	{
		const size_t count = m_slots.size();
		SignalDetail::Slots::EmitScope scope( m_slots );
		for ( size_t i = 0; i < count; ++i )
		{
			SignalDetail::Node3<A, B, C> *node = static_cast<SignalDetail::Node3<A, B, C> *>( m_slots.at( i ) );
			if ( node->alive )
				node->call( a, b, c );
		}
	}

	bool hasListeners() const { return m_slots.hasListeners(); }

private:
	SignalDetail::Slots m_slots;
};

// Yes/no question to whoever is listening (e.g. "is the screen already leaving?"): any() is true
// as soon as one live listener answers true, false when there are none.
class Predicate0
{
public:
	typedef SignalConnection Connection;

	template<class F> Connection connect( F fn )
	{
		SignalDetail::FnBool0<F> *node = new SignalDetail::FnBool0<F>( fn );
		m_slots.add( node );
		return Connection( node );
	}

	bool any()
	{
		const size_t count = m_slots.size();
		SignalDetail::Slots::EmitScope scope( m_slots );
		for ( size_t i = 0; i < count; ++i )
		{
			SignalDetail::NodeBool0 *node = static_cast<SignalDetail::NodeBool0 *>( m_slots.at( i ) );
			if ( node->alive && node->call() )
				return true;
		}
		return false;
	}

	bool hasListeners() const { return m_slots.hasListeners(); }

private:
	SignalDetail::Slots m_slots;
};
