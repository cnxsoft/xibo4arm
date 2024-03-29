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
//

#ifndef _GPUInvertFilter_H_
#define _GPUInvertFilter_H_

#include "../api.h"

#include "GPUFilter.h"
#include "GLShaderParam.h"

namespace avg {

class AVG_API GPUInvertFilter : public GPUFilter
{
public:
    GPUInvertFilter(const IntPoint& size, PixelFormat pf,
            bool bStandalone=true);
    virtual ~GPUInvertFilter();

    virtual void applyOnGPU(GLTexturePtr pSrcTex);
    void initShader();

private:
    IntGLShaderParamPtr m_pTextureParam;
};

typedef boost::shared_ptr<GPUInvertFilter> GPUInvertFilterPtr;

} //end namespace avg
#endif
