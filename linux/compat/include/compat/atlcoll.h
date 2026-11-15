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
	static ULONG Hash(INARGTYPE s) { ULONG n = 0; for ( ; s && *s; ++s ) n = ( n << 5 ) + n + *s; return n; }
	static bool CompareElements(INARGTYPE a, INARGTYPE b) { return T( a ) == b; }
};

template< typename T > struct CStringElementTraitsI : CElementTraits< T >
{
	typedef const typename T::XCHAR* INARGTYPE;
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

template< typename K, typename V, class KTraits = CElementTraits< K >, class VTraits = CElementTraits< V > >
class CAtlMap
{
	struct KeyHash { size_t operator()(const K& k) const { return KTraits::Hash( k ); } };
	struct KeyEq { bool operator()(const K& a, const K& b) const { return KTraits::CompareElements( a, b ); } };
	typedef std::unordered_map< K, V, KeyHash, KeyEq > Map;
public:
	class CPair
	{
	public:
		const K m_key;
		V m_value;
	};
	size_t GetCount() const { return m_map.size(); }
	bool IsEmpty() const { return m_map.empty(); }
	bool Lookup(typename KTraits::INARGTYPE key, V& value) const
	{
		auto i = m_map.find( K( key ) );
		if ( i == m_map.end() ) return false;
		value = i->second;
		return true;
	}
	POSITION SetAt(typename KTraits::INARGTYPE key, typename VTraits::INARGTYPE value)
	{
		m_map[ K( key ) ] = value;
		return (POSITION)1;
	}
	V& operator[](typename KTraits::INARGTYPE key) { return m_map[ K( key ) ]; }
	bool RemoveKey(typename KTraits::INARGTYPE key) { return m_map.erase( K( key ) ) != 0; }
	void RemoveAll() { m_map.clear(); }
	void InitHashTable(UINT nBins, bool = true) { m_map.reserve( nBins ); }
private:
	Map m_map;
};

template< typename E, class ETraits = CElementTraits< E > >
class CAtlArray : public CArray< E, typename ETraits::INARGTYPE >
{
};
