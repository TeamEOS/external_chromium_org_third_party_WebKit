/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008, 2009, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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
 */

#include "config.h"
#include "core/dom/DocumentStyleSheetCollection.h"

#include "HTMLNames.h"
#include "SVGNames.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleInvalidationAnalysis.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/dom/WebCoreMemoryInstrumentation.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/page/Page.h"
#include "core/page/PageGroup.h"
#include "core/page/Settings.h"
#include "core/page/UserContentURLPattern.h"
#include "core/svg/SVGStyleElement.h"
#include <wtf/MemoryInstrumentationListHashSet.h>
#include <wtf/MemoryInstrumentationVector.h>

namespace WebCore {

using namespace HTMLNames;

DocumentStyleSheetCollection::DocumentStyleSheetCollection(Document* document)
    : m_document(document)
    , m_pendingStylesheets(0)
    , m_injectedStyleSheetCacheValid(false)
    , m_hadActiveLoadingStylesheet(false)
    , m_needsUpdateActiveStylesheetsOnStyleRecalc(false)
    , m_usesSiblingRules(false)
    , m_usesSiblingRulesOverride(false)
    , m_usesFirstLineRules(false)
    , m_usesFirstLetterRules(false)
    , m_usesBeforeAfterRules(false)
    , m_usesBeforeAfterRulesOverride(false)
    , m_usesRemUnits(false)
{
}

DocumentStyleSheetCollection::~DocumentStyleSheetCollection()
{
    if (m_pageUserSheet)
        m_pageUserSheet->clearOwnerNode();
    for (unsigned i = 0; i < m_injectedUserStyleSheets.size(); ++i)
        m_injectedUserStyleSheets[i]->clearOwnerNode();
    for (unsigned i = 0; i < m_injectedAuthorStyleSheets.size(); ++i)
        m_injectedAuthorStyleSheets[i]->clearOwnerNode();
    for (unsigned i = 0; i < m_userStyleSheets.size(); ++i)
        m_userStyleSheets[i]->clearOwnerNode();
    for (unsigned i = 0; i < m_authorStyleSheets.size(); ++i)
        m_authorStyleSheets[i]->clearOwnerNode();
}

void DocumentStyleSheetCollection::combineCSSFeatureFlags()
{
    // Delay resetting the flags until after next style recalc since unapplying the style may not work without these set (this is true at least with before/after).
    StyleResolver* styleResolver = m_document->styleResolver();
    m_usesSiblingRules = m_usesSiblingRules || styleResolver->usesSiblingRules();
    m_usesFirstLineRules = m_usesFirstLineRules || styleResolver->usesFirstLineRules();
    m_usesBeforeAfterRules = m_usesBeforeAfterRules || styleResolver->usesBeforeAfterRules();
}

void DocumentStyleSheetCollection::resetCSSFeatureFlags()
{
    StyleResolver* styleResolver = m_document->styleResolver();
    m_usesSiblingRules = styleResolver->usesSiblingRules();
    m_usesFirstLineRules = styleResolver->usesFirstLineRules();
    m_usesBeforeAfterRules = styleResolver->usesBeforeAfterRules();
}

CSSStyleSheet* DocumentStyleSheetCollection::pageUserSheet()
{
    if (m_pageUserSheet)
        return m_pageUserSheet.get();
    
    Page* owningPage = m_document->page();
    if (!owningPage)
        return 0;
    
    String userSheetText = owningPage->userStyleSheet();
    if (userSheetText.isEmpty())
        return 0;
    
    // Parse the sheet and cache it.
    m_pageUserSheet = CSSStyleSheet::createInline(m_document, m_document->settings()->userStyleSheetLocation());
    m_pageUserSheet->contents()->setIsUserStyleSheet(true);
    m_pageUserSheet->contents()->parseString(userSheetText);
    return m_pageUserSheet.get();
}

void DocumentStyleSheetCollection::clearPageUserSheet()
{
    if (m_pageUserSheet) {
        m_pageUserSheet = 0;
        m_document->styleResolverChanged(DeferRecalcStyle);
    }
}

void DocumentStyleSheetCollection::updatePageUserSheet()
{
    clearPageUserSheet();
    if (pageUserSheet())
        m_document->styleResolverChanged(RecalcStyleImmediately);
}

const Vector<RefPtr<CSSStyleSheet> >& DocumentStyleSheetCollection::injectedUserStyleSheets() const
{
    updateInjectedStyleSheetCache();
    return m_injectedUserStyleSheets;
}

const Vector<RefPtr<CSSStyleSheet> >& DocumentStyleSheetCollection::injectedAuthorStyleSheets() const
{
    updateInjectedStyleSheetCache();
    return m_injectedAuthorStyleSheets;
}

void DocumentStyleSheetCollection::updateInjectedStyleSheetCache() const
{
    if (m_injectedStyleSheetCacheValid)
        return;
    m_injectedStyleSheetCacheValid = true;
    m_injectedUserStyleSheets.clear();
    m_injectedAuthorStyleSheets.clear();

    Page* owningPage = m_document->page();
    if (!owningPage)
        return;

    const PageGroup& pageGroup = owningPage->group();
    const UserStyleSheetVector& sheets = pageGroup.userStyleSheets();
    for (unsigned i = 0; i < sheets.size(); ++i) {
        const UserStyleSheet* sheet = sheets[i].get();
        if (sheet->injectedFrames() == InjectInTopFrameOnly && m_document->ownerElement())
            continue;
        if (!UserContentURLPattern::matchesPatterns(m_document->url(), sheet->whitelist(), sheet->blacklist()))
            continue;
        RefPtr<CSSStyleSheet> groupSheet = CSSStyleSheet::createInline(const_cast<Document*>(m_document), sheet->url());
        bool isUserStyleSheet = sheet->level() == UserStyleUserLevel;
        if (isUserStyleSheet)
            m_injectedUserStyleSheets.append(groupSheet);
        else
            m_injectedAuthorStyleSheets.append(groupSheet);
        groupSheet->contents()->setIsUserStyleSheet(isUserStyleSheet);
        groupSheet->contents()->parseString(sheet->source());
    }
}

void DocumentStyleSheetCollection::invalidateInjectedStyleSheetCache()
{
    m_injectedStyleSheetCacheValid = false;
    m_document->styleResolverChanged(DeferRecalcStyle);
}

void DocumentStyleSheetCollection::addAuthorSheet(PassRefPtr<StyleSheetContents> authorSheet)
{
    ASSERT(!authorSheet->isUserStyleSheet());
    m_authorStyleSheets.append(CSSStyleSheet::create(authorSheet, m_document));
    m_document->styleResolverChanged(RecalcStyleImmediately);
}

void DocumentStyleSheetCollection::addUserSheet(PassRefPtr<StyleSheetContents> userSheet)
{
    ASSERT(userSheet->isUserStyleSheet());
    m_userStyleSheets.append(CSSStyleSheet::create(userSheet, m_document));
    m_document->styleResolverChanged(RecalcStyleImmediately);
}

// This method is called whenever a top-level stylesheet has finished loading.
void DocumentStyleSheetCollection::removePendingSheet(RemovePendingSheetNotificationType notification)
{
    // Make sure we knew this sheet was pending, and that our count isn't out of sync.
    ASSERT(m_pendingStylesheets > 0);

    m_pendingStylesheets--;
    
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Stylesheet loaded at time %d. %d stylesheets still remain.\n", elapsedTime(), m_pendingStylesheets);
#endif

    if (m_pendingStylesheets)
        return;

    if (notification == RemovePendingSheetNotifyLater) {
        m_document->setNeedsNotifyRemoveAllPendingStylesheet();
        return;
    }
    
    m_document->didRemoveAllPendingStylesheet();
}

void DocumentStyleSheetCollection::addStyleSheetCandidateNode(Node* node, bool createdByParser)
{
    if (!node->inDocument())
        return;

    // Until the <body> exists, we have no choice but to compare document positions,
    // since styles outside of the body and head continue to be shunted into the head
    // (and thus can shift to end up before dynamically added DOM content that is also
    // outside the body).
    if (createdByParser && m_document->body()) {
        m_styleSheetCandidateNodes.parserAdd(node);
        return;
    }

    m_styleSheetCandidateNodes.add(node);
}

void DocumentStyleSheetCollection::removeStyleSheetCandidateNode(Node* node)
{
    m_styleSheetCandidateNodes.remove(node);
}

void DocumentStyleSheetCollection::collectStyleSheets(Vector<RefPtr<StyleSheet> >& styleSheets, Vector<RefPtr<CSSStyleSheet> >& activeSheets)
{
    if (m_document->settings() && !m_document->settings()->authorAndUserStylesEnabled())
        return;

    DocumentOrderedList::iterator begin = m_styleSheetCandidateNodes.begin();
    DocumentOrderedList::iterator end = m_styleSheetCandidateNodes.end();
    for (DocumentOrderedList::iterator it = begin; it != end; ++it) {
        Node* n = *it;
        StyleSheet* sheet = 0;
        CSSStyleSheet* activeSheet = 0;
        if (n->nodeType() == Node::PROCESSING_INSTRUCTION_NODE) {
            // Processing instruction (XML documents only).
            // We don't support linking to embedded CSS stylesheets, see <https://bugs.webkit.org/show_bug.cgi?id=49281> for discussion.
            ProcessingInstruction* pi = static_cast<ProcessingInstruction*>(n);
            // Don't apply XSL transforms to already transformed documents -- <rdar://problem/4132806>
            if (pi->isXSL() && !m_document->transformSourceDocument()) {
                // Don't apply XSL transforms until loading is finished.
                if (!m_document->parsing())
                    m_document->applyXSLTransform(pi);
                return;
            }
            sheet = pi->sheet();
            if (sheet && !sheet->disabled() && sheet->isCSSStyleSheet())
                activeSheet = static_cast<CSSStyleSheet*>(sheet);
        } else if ((n->isHTMLElement() && (n->hasTagName(linkTag) || n->hasTagName(styleTag))) || (n->isSVGElement() && n->hasTagName(SVGNames::styleTag))) {
            Element* e = toElement(n);
            AtomicString title = e->getAttribute(titleAttr);
            bool enabledViaScript = false;
            if (e->hasLocalName(linkTag)) {
                // <LINK> element
                HTMLLinkElement* linkElement = static_cast<HTMLLinkElement*>(n);
                enabledViaScript = linkElement->isEnabledViaScript();
                if (!linkElement->isDisabled() && linkElement->styleSheetIsLoading()) {
                    // it is loading but we should still decide which style sheet set to use
                    if (!enabledViaScript && !title.isEmpty() && m_preferredStylesheetSetName.isEmpty()) {
                        const AtomicString& rel = e->getAttribute(relAttr);
                        if (!rel.contains("alternate")) {
                            m_preferredStylesheetSetName = title;
                            m_selectedStylesheetSetName = title;
                        }
                    }

                    continue;
                }
                sheet = linkElement->sheet();
                if (!sheet)
                    title = nullAtom;
            } else if (n->isSVGElement() && n->hasTagName(SVGNames::styleTag)) {
                sheet = static_cast<SVGStyleElement*>(n)->sheet();
            } else {
                sheet = static_cast<HTMLStyleElement*>(n)->sheet();
            }

            if (sheet && !sheet->disabled() && sheet->isCSSStyleSheet())
                activeSheet = static_cast<CSSStyleSheet*>(sheet);

            // Check to see if this sheet belongs to a styleset
            // (thus making it PREFERRED or ALTERNATE rather than
            // PERSISTENT).
            AtomicString rel = e->getAttribute(relAttr);
            if (!enabledViaScript && sheet && !title.isEmpty()) {
                // Yes, we have a title.
                if (m_preferredStylesheetSetName.isEmpty()) {
                    // No preferred set has been established. If
                    // we are NOT an alternate sheet, then establish
                    // us as the preferred set. Otherwise, just ignore
                    // this sheet.
                    if (e->hasLocalName(styleTag) || !rel.contains("alternate"))
                        m_preferredStylesheetSetName = m_selectedStylesheetSetName = title;
                }
                if (title != m_preferredStylesheetSetName)
                    activeSheet = 0;
            }

            if (rel.contains("alternate") && title.isEmpty())
                activeSheet = 0;
        }
        if (sheet)
            styleSheets.append(sheet);
        if (activeSheet)
            activeSheets.append(activeSheet);
    }
}

void DocumentStyleSheetCollection::analyzeStyleSheetChange(StyleResolverUpdateMode updateMode, const Vector<RefPtr<CSSStyleSheet> >& newStylesheets, StyleResolverUpdateType& styleResolverUpdateType, bool& requiresFullStyleRecalc)
{
    styleResolverUpdateType = Reconstruct;
    requiresFullStyleRecalc = true;

    // Stylesheets of <style> elements that @import stylesheets are active but loading. We need to trigger a full recalc when such loads are done.
    bool hasActiveLoadingStylesheet = false;
    unsigned newStylesheetCount = newStylesheets.size();
    for (unsigned i = 0; i < newStylesheetCount; ++i) {
        if (newStylesheets[i]->isLoading())
            hasActiveLoadingStylesheet = true;
    }
    if (m_hadActiveLoadingStylesheet && !hasActiveLoadingStylesheet) {
        m_hadActiveLoadingStylesheet = false;
        return;
    }
    m_hadActiveLoadingStylesheet = hasActiveLoadingStylesheet;

    if (updateMode != AnalyzedStyleUpdate)
        return;
    if (!m_document->styleResolverIfExists())
        return;

    // Find out which stylesheets are new.
    unsigned oldStylesheetCount = m_activeAuthorStyleSheets.size();
    if (newStylesheetCount < oldStylesheetCount)
        return;
    Vector<StyleSheetContents*> addedSheets;
    unsigned newIndex = 0;
    for (unsigned oldIndex = 0; oldIndex < oldStylesheetCount; ++oldIndex) {
        if (newIndex >= newStylesheetCount)
            return;
        while (m_activeAuthorStyleSheets[oldIndex] != newStylesheets[newIndex]) {
            addedSheets.append(newStylesheets[newIndex]->contents());
            ++newIndex;
            if (newIndex == newStylesheetCount)
                return;
        }
        ++newIndex;
    }
    bool hasInsertions = !addedSheets.isEmpty();
    while (newIndex < newStylesheetCount) {
        addedSheets.append(newStylesheets[newIndex]->contents());
        ++newIndex;
    }
    // If all new sheets were added at the end of the list we can just add them to existing StyleResolver.
    // If there were insertions we need to re-add all the stylesheets so rules are ordered correctly.
    styleResolverUpdateType = hasInsertions ? Reset : Additive;

    // If we are already parsing the body and so may have significant amount of elements, put some effort into trying to avoid style recalcs.
    if (!m_document->body() || m_document->hasNodesWithPlaceholderStyle())
        return;
    StyleInvalidationAnalysis invalidationAnalysis(addedSheets);
    if (invalidationAnalysis.dirtiesAllStyle())
        return;
    invalidationAnalysis.invalidateStyle(m_document);
    requiresFullStyleRecalc = false;
}

static bool styleSheetsUseRemUnits(const Vector<RefPtr<CSSStyleSheet> >& sheets)
{
    for (unsigned i = 0; i < sheets.size(); ++i) {
        if (sheets[i]->contents()->usesRemUnits())
            return true;
    }
    return false;
}

static void collectActiveCSSStyleSheetsFromSeamlessParents(Vector<RefPtr<CSSStyleSheet> >& sheets, Document* document)
{
    HTMLIFrameElement* seamlessParentIFrame = document->seamlessParentIFrame();
    if (!seamlessParentIFrame)
        return;
    sheets.append(seamlessParentIFrame->document()->styleSheetCollection()->activeAuthorStyleSheets());
}

bool DocumentStyleSheetCollection::updateActiveStyleSheets(StyleResolverUpdateMode updateMode)
{
    if (m_document->inStyleRecalc()) {
        // SVG <use> element may manage to invalidate style selector in the middle of a style recalc.
        // https://bugs.webkit.org/show_bug.cgi?id=54344
        // FIXME: This should be fixed in SVG and the call site replaced by ASSERT(!m_inStyleRecalc).
        m_needsUpdateActiveStylesheetsOnStyleRecalc = true;
        m_document->scheduleForcedStyleRecalc();
        return false;

    }
    if (!m_document->renderer() || !m_document->attached())
        return false;

    Vector<RefPtr<StyleSheet> > styleSheets;
    Vector<RefPtr<CSSStyleSheet> > activeCSSStyleSheets;
    activeCSSStyleSheets.append(injectedAuthorStyleSheets());
    activeCSSStyleSheets.append(documentAuthorStyleSheets());
    collectActiveCSSStyleSheetsFromSeamlessParents(activeCSSStyleSheets, m_document);
    collectStyleSheets(styleSheets, activeCSSStyleSheets);

    StyleResolverUpdateType styleResolverUpdateType;
    bool requiresFullStyleRecalc;
    analyzeStyleSheetChange(updateMode, activeCSSStyleSheets, styleResolverUpdateType, requiresFullStyleRecalc);

    if (styleResolverUpdateType == Reconstruct)
        m_document->clearStyleResolver();
    else {
        StyleResolver* styleResolver = m_document->styleResolver();
        if (styleResolverUpdateType == Reset) {
            styleResolver->resetAuthorStyle();
            styleResolver->appendAuthorStyleSheets(0, activeCSSStyleSheets);
        } else {
            ASSERT(styleResolverUpdateType == Additive);
            styleResolver->appendAuthorStyleSheets(m_activeAuthorStyleSheets.size(), activeCSSStyleSheets);
        }
        resetCSSFeatureFlags();
    }
    m_activeAuthorStyleSheets.swap(activeCSSStyleSheets);
    InspectorInstrumentation::activeStyleSheetsUpdated(m_document, styleSheets);
    m_styleSheetsForStyleSheetList.swap(styleSheets);

    m_usesRemUnits = styleSheetsUseRemUnits(m_activeAuthorStyleSheets);
    m_needsUpdateActiveStylesheetsOnStyleRecalc = false;

    m_document->notifySeamlessChildDocumentsOfStylesheetUpdate();

    return requiresFullStyleRecalc;
}

void DocumentStyleSheetCollection::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
    info.addMember(m_pageUserSheet, "pageUserSheet");
    info.addMember(m_injectedUserStyleSheets, "injectedUserStyleSheets");
    info.addMember(m_injectedAuthorStyleSheets, "injectedAuthorStyleSheets");
    info.addMember(m_userStyleSheets, "userStyleSheets");
    info.addMember(m_authorStyleSheets, "authorStyleSheets");
    info.addMember(m_activeAuthorStyleSheets, "activeAuthorStyleSheets");
    info.addMember(m_styleSheetsForStyleSheetList, "styleSheetsForStyleSheetList");
    info.addMember(m_styleSheetCandidateNodes, "styleSheetCandidateNodes");
    info.addMember(m_preferredStylesheetSetName, "preferredStylesheetSetName");
    info.addMember(m_selectedStylesheetSetName, "selectedStylesheetSetName");
    info.addMember(m_document, "document");
}

}
