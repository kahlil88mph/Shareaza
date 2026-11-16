//
// compat/atlcoll.h (Linux compatibility layer)
//
// ATL collections (CAtlList, CAtlMap, CAtlArray) and element traits.
//
// This file is part of the Linux port of SHAREAZA and is GPL v2 or later.
//

#pragma once

#include "afxcore.h"
#include <unordered_map>

template< typename T > struct CElementTraits
{
	typedef const T& INARGTYPE;
	typedef T& OUTARGTYPE;
	static ULONG Hash(const T& v) { return (ULONG)std::hash< T >()( v ); }
	static bool CompareElements(const T& a, const T& b) { return a == b; }
};

template< typename T > struct CStringElementTraits : CElementTraits< T >
{
	typedef const typename T::XCHAR* INARGTYPE;
	typedef T& OUTARGTYPE;
	static ULONG Hash(INARGTYPE s) { ULONG n = 0; for ( ; s && *s; ++s ) n = ( n << 5 ) + n + *s; return n; }
	static bool CompareElements(INARGTYPE a, INARGTYPE b) { return T( a ) == b; }
};

template< typename T > struct CStringElementTraitsI : CElementTraits< T >
{
	typedef const typename T::XCHAR* INARGTYPE;
	typedef T& OUTARGTYPE;
	static ULONG Hash(INARGTYPE s)
	{
		ULONG n = 0;
		for ( ; s && *s; ++s ) n = ( n << 5 ) + n + (ULONG)towlower( *s );
		return n;
	}
	static bool CompareElements(INARGTYPE a, INARGTYPE b) { return T( a ).CompareNoCase( b ) == 0; }
};

template< typename E, class ETraits = CElementTraits< E > >
class CAtlList : public CList< E, typename ETraits::INARGTYPE >
{
};

template< typename T > struct CStringRefElementTraits : CStringElementTraits< T > {};
template< typename T > struct CPrimitiveElementTraits : CElementTraits< T > {};
template< typename T > struct CDefaultElementTraits : CElementTraits< T > {};

// CAtlMap: a hash map whose POSITIONs walk all entries, as ATL's does.
// Entries are nodes in a list (for POSITION) indexed by a std::unordered_map.
template< typename K, typename V, class KTraits = CElementTraits< K >, class VTraits = CElementTraits< V > >
class CAtlMap
{
public:
	typedef typename KTraits::INARGTYPE KINARGTYPE;
	typedef typename VTraits::INARGTYPE VINARGTYPE;

	class CPair
	{
	public:
		const K m_key;
		V m_value;
	protected:
		explicit CPair(KINARGTYPE key) : m_key( key ), m_value() {}
		friend class CAtlMap;
	};
private:
	class CNode : public CPair
	{
	public:
		explicit CNode(KINARGTYPE key) : CPair( key ), pPrev( NULL ), pNext( NULL ) {}
		CNode* pPrev;
		CNode* pNext;
	};
	struct KeyHash { size_t operator()(const K& k) const { return KTraits::Hash( k ); } };
	struct KeyEq { bool operator()(const K& a, const K& b) const { return KTraits::CompareElements( a, b ); } };

public:
	explicit CAtlMap(UINT = 17, float = 0.75f, float = 0.25f, float = 2.25f, UINT = 10) : m_pHead( NULL ), m_pTail( NULL ) {}
	~CAtlMap() { RemoveAll(); }

	size_t GetCount() const { return m_index.size(); }
	bool IsEmpty() const { return m_index.empty(); }
	void InitHashTable(UINT nBins, bool = true) { m_index.reserve( nBins ); }

	bool Lookup(KINARGTYPE key, typename VTraits::OUTARGTYPE value) const
	{
		const CNode* p = Find( key );
		if ( ! p ) return false;
		value = p->m_value;
		return true;
	}
	const CPair* Lookup(KINARGTYPE key) const { return Find( key ); }
	CPair* Lookup(KINARGTYPE key) { return const_cast< CNode* >( Find( key ) ); }
	V& operator[](KINARGTYPE key) { return Get( key )->m_value; }
	POSITION SetAt(KINARGTYPE key, VINARGTYPE value)
	{
		CNode* p = Get( key );
		p->m_value = value;
		return (POSITION)p;
	}
	bool RemoveKey(KINARGTYPE key)
	{
		auto i = m_index.find( K( key ) );
		if ( i == m_index.end() ) return false;
		Unlink( i->second );
		m_index.erase( i );
		return true;
	}
	void RemoveAtPos(POSITION pos)
	{
		CNode* p = (CNode*)pos;
		m_index.erase( p->m_key );
		Unlink( p );
	}
	void RemoveAll()
	{
		for ( CNode* p = m_pHead; p; ) { CNode* n = p->pNext; delete p; p = n; }
		m_pHead = m_pTail = NULL;
		m_index.clear();
	}

	POSITION GetStartPosition() const { return (POSITION)m_pHead; }
	void GetNextAssoc(POSITION& pos, typename KTraits::OUTARGTYPE key, typename VTraits::OUTARGTYPE value) const
	{
		CNode* p = (CNode*)pos;
		key = p->m_key;
		value = p->m_value;
		pos = (POSITION)p->pNext;
	}
	CPair* GetNext(POSITION& pos) { CNode* p = (CNode*)pos; pos = (POSITION)p->pNext; return p; }
	const CPair* GetNext(POSITION& pos) const { CNode* p = (CNode*)pos; pos = (POSITION)p->pNext; return p; }
	const K& GetNextKey(POSITION& pos) const { CNode* p = (CNode*)pos; pos = (POSITION)p->pNext; return p->m_key; }
	V& GetNextValue(POSITION& pos) { CNode* p = (CNode*)pos; pos = (POSITION)p->pNext; return p->m_value; }
	const V& GetNextValue(POSITION& pos) const { CNode* p = (CNode*)pos; pos = (POSITION)p->pNext; return p->m_value; }
	void GetAt(POSITION pos, typename KTraits::OUTARGTYPE key, typename VTraits::OUTARGTYPE value) const
	{
		key = ( (CNode*)pos )->m_key;
		value = ( (CNode*)pos )->m_value;
	}
	CPair* GetAt(POSITION pos) { return (CNode*)pos; }
	const K& GetKeyAt(POSITION pos) const { return ( (CNode*)pos )->m_key; }
	V& GetValueAt(POSITION pos) { return ( (CNode*)pos )->m_value; }
	const V& GetValueAt(POSITION pos) const { return ( (CNode*)pos )->m_value; }
	void SetValueAt(POSITION pos, VINARGTYPE value) { ( (CNode*)pos )->m_value = value; }

private:
	std::unordered_map< K, CNode*, KeyHash, KeyEq > m_index;
	CNode* m_pHead;
	CNode* m_pTail;

	const CNode* Find(KINARGTYPE key) const
	{
		auto i = m_index.find( K( key ) );
		return i == m_index.end() ? NULL : i->second;
	}
	CNode* Get(KINARGTYPE key)
	{
		auto i = m_index.find( K( key ) );
		if ( i != m_index.end() ) return i->second;
		CNode* p = new CNode( key );
		p->pPrev = m_pTail;
		if ( m_pTail ) m_pTail->pNext = p; else m_pHead = p;
		m_pTail = p;
		m_index.emplace( p->m_key, p );
		return p;
	}
	void Unlink(CNode* p)
	{
		if ( p->pPrev ) p->pPrev->pNext = p->pNext; else m_pHead = p->pNext;
		if ( p->pNext ) p->pNext->pPrev = p->pPrev; else m_pTail = p->pPrev;
		delete p;
	}
	CAtlMap(const CAtlMap&);
	CAtlMap& operator=(const CAtlMap&);
};

template< typename E, class ETraits = CElementTraits< E > >
class CAtlArray : public CArray< E, typename ETraits::INARGTYPE >
{
};
