/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef StyleResourceLoader_h
#define StyleResourceLoader_h

#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

class ElementStyleResources;
class CachedResourceLoader;
class RenderStyle;
class ShapeValue;
class StyleImage;
class StylePendingImage;
class StyleCustomFilterProgramCache;

// Manages loading of resources, requested by the stylesheets.
// Expects the same lifetime as StyleResolver, because:
// 1) it expects CachedResourceLoader to never change, and
// 2) it also holds the StyleCustomFilterProgramCache.
class StyleResourceLoader {
WTF_MAKE_NONCOPYABLE(StyleResourceLoader);
public:
    explicit StyleResourceLoader(CachedResourceLoader*);

    void loadPendingResources(RenderStyle*, ElementStyleResources&);
    StyleCustomFilterProgramCache* customFilterProgramCache() const { return m_customFilterProgramCache.get(); }

private:
    void loadPendingSVGDocuments(RenderStyle*, const ElementStyleResources&);
    void loadPendingShaders(RenderStyle*, const ElementStyleResources&);

    PassRefPtr<StyleImage> loadPendingImage(StylePendingImage*, float deviceScaleFactor);
    void loadPendingImages(RenderStyle*, const ElementStyleResources&);
    void loadPendingShapeImage(RenderStyle*, ShapeValue*);

    OwnPtr<StyleCustomFilterProgramCache> m_customFilterProgramCache;
    CachedResourceLoader* m_cachedResourceLoader;
};

} // namespace WebCore

#endif // StyleResourceLoader_h
