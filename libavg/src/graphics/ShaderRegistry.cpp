//
//  libavg - Media Playback Engine. 
//  Copyright (C) 2003-2011 Ulrich von Zadow
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//  Current versions can be found at www.libavg.de
//

#include "ShaderRegistry.h"

#include "GLContext.h"

#include "../base/Logger.h"
#include "../base/Exception.h"
#include "../base/OSHelper.h"
#include "../base/FileHelper.h"
#include "../base/StringHelper.h"

#include <iostream>

using namespace std;
using namespace boost;

namespace avg {

std::string ShaderRegistry::m_sLibPath;
    
ShaderRegistryPtr ShaderRegistry::get() 
{
    return GLContext::getCurrent()->getShaderRegistry();
}

ShaderRegistry::ShaderRegistry()
{
    if (m_sLibPath == "") {
        m_sLibPath = getPath(getAvgLibPath())+"shaders";
#ifdef __linux
        // XXX: If we're running make distcheck, the shaders are in a different place than
        // usual. Grrr.
        char * pszSrcDir = getenv("srcdir");
        if (pszSrcDir && string(pszSrcDir) != ".") {
            m_sLibPath = string(pszSrcDir) + "/../graphics/shaders";
        }
#endif
        AVG_TRACE(Logger::CONFIG, "Loading shaders from "+m_sLibPath);
    }
}

ShaderRegistry::~ShaderRegistry() 
{
}

void ShaderRegistry::setShaderPath(const std::string& sLibPath)
{
    m_sLibPath = sLibPath;
#ifdef __linux
    // XXX: If we're running make distcheck, the shaders are in a different place than
    // usual. Grrr.
    char * pszSrcDir = getenv("srcdir");
    if (pszSrcDir && string(pszSrcDir) != ".") {
        m_sLibPath = string(pszSrcDir) + "/../graphics/shaders";
    }
#endif
    AVG_TRACE(Logger::CONFIG, "Loading shaders from "+m_sLibPath);
}

void ShaderRegistry::setPreprocessorDefine(const std::string& sName, 
        const std::string& sValue)
{
    m_PreprocessorDefinesMap[sName] = sValue;
}

void ShaderRegistry::createShader(const std::string& sID)
{
    string sShaderCode;
    string sFileName = m_sLibPath+"/"+sID+".frag";
    readWholeFile(sFileName, sShaderCode);
    string sPreprocessed;
    preprocess(sShaderCode, sFileName, sPreprocessed);
    string sDefines = createDefinesString();
    OGLShaderPtr pShader = getShader(sID);
    if (!pShader) {
        m_ShaderMap[sID] = OGLShaderPtr(new OGLShader(sID, sPreprocessed, sDefines));
    }
}

OGLShaderPtr ShaderRegistry::getShader(const std::string& sID) const
{
    ShaderMap::const_iterator it = m_ShaderMap.find(sID);
    if (it == m_ShaderMap.end()) {
        return OGLShaderPtr();
    } else {
        return it->second;
    }
}

OGLShaderPtr ShaderRegistry::getCurShader() const
{
    return m_pCurShader;
}

void ShaderRegistry::setCurShader(const std::string& sID)
{
    if (sID == "") {
        m_pCurShader = OGLShaderPtr();
    } else {
        m_pCurShader = getShader(sID);
    }
}

void ShaderRegistry::preprocess(const string& sShaderCode, const string& sFileName, 
        string& sProcessed)
{
    sProcessed.append("#line 0\n");
    istringstream stream(sShaderCode);
    string sCurLine;
    int curLine = 0;
    while(getline(stream, sCurLine)) {
        curLine++;
        string sStripped = removeStartEndSpaces(sCurLine);
        if (sStripped.substr(0, 8) == "#include") {
            size_t startPos = sStripped.find('"');
            size_t endPos = sStripped.find('"', startPos+1);
            if (startPos == string::npos || endPos == string::npos) {
                throwParseError(sFileName, curLine);
            }
            string sIncFileName = sStripped.substr(startPos+1, endPos-startPos-1);
            sIncFileName = m_sLibPath+"/"+sIncFileName;
            string sIncludedFile;
            readWholeFile(sIncFileName, sIncludedFile);
            string sProcessedIncludedFile;
            preprocess(sIncludedFile, sIncFileName, sProcessedIncludedFile);
            sProcessed.append(sProcessedIncludedFile);
            sProcessed.append("#line "+toString(curLine)+"\n");
        } else {
            sProcessed.append(sCurLine+"\n");
        }
    }
}

string ShaderRegistry::createDefinesString()
{
    stringstream ss;
    std::map<std::string, std::string>::iterator it;
    for (it=m_PreprocessorDefinesMap.begin(); it != m_PreprocessorDefinesMap.end();
            ++it)
    {
        ss << "#define " << it->first << " " << it->second << endl;
    }
    return ss.str();
}

void ShaderRegistry::throwParseError(const string& sFileName, int curLine)
{
    throw Exception(AVG_ERR_VIDEO_GENERAL, "File '"+sFileName+"', Line "+
            toString(curLine)+": Syntax error.");
}

void createShader(const std::string& sID)
{
    return ShaderRegistry::get()->createShader(sID);
}

OGLShaderPtr getShader(const std::string& sID)
{
    return ShaderRegistry::get()->getShader(sID);
}

}

