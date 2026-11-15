// Resource string table for CString::LoadString (Linux compatibility layer).

#include <compat/cstring.h>
#include <map>
#include <mutex>

namespace compat {

namespace {
	std::mutex g_oLock;
	std::map< UINT, std::wstring >& Table()
	{
		static std::map< UINT, std::wstring > t;
		return t;
	}
}

const wchar_t* GetResourceString(UINT nID)
{
	std::lock_guard< std::mutex > l( g_oLock );
	auto i = Table().find( nID );
	return i == Table().end() ? NULL : i->second.c_str();
}

void SetResourceString(UINT nID, const wchar_t* pszText)
{
	std::lock_guard< std::mutex > l( g_oLock );
	Table()[ nID ] = pszText ? pszText : L"";
}

} // namespace compat
