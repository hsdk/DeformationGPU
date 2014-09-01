//   Copyright 2013 Henry Schäfer
//
//   Licensed under the Apache License, Version 2.0 (the "Apache License")
//
//   You may obtain a copy of the Apache License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the Apache License  is  distributed on an 
//   "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//	 either express or implied. See the Apache License for the specific
//   language governing permissions and limitations under the Apache License.
//

#pragma once


__forceinline std::string GetFileExt(const std::string& file)
{
	size_t found = file.find_last_of(".");
	return file.substr(found+1);
}

__forceinline std::string ReplaceFileExtension(const std::string& file, const std::string& ext)
{
	size_t found = file.find_last_of(".");
	return file.substr(0,found) + ext;
}

__forceinline std::string ExtractPath(const std::string& fname)
{
	size_t pos = fname.find_last_of("\\/");
	return (std::string::npos == pos)	? "" : fname.substr(0, pos);
}

__forceinline std::wstring ExtractPath(const std::wstring& fname)
{	
	size_t pos = fname.find_last_of(L"\\/");
	return (std::wstring::npos == pos)	? L""	: fname.substr(0, pos);
}

__forceinline std::string utf8_encode(const std::wstring &wstr)
{
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo( size_needed, 0 );
	WideCharToMultiByte (CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

__forceinline std::wstring string2wstring(const std::string& str)
{
	std::wstring wstr (str.begin(), str.end());
	return wstr;
}

__forceinline std::wstring s2ws(const std::string& s)
{
	int len;
	int slength = (int)s.length() + 1;
	len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	std::wstring r(buf);
	delete[] buf;
	return r;
}

