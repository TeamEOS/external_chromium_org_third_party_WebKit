/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/Node.h"

#include "V8Attr.h"
#include "V8CDATASection.h"
#include "V8Comment.h"
#include "V8Document.h"
#include "V8DocumentFragment.h"
#include "V8DocumentType.h"
#include "V8Element.h"
#include "V8Entity.h"
#include "V8HTMLElement.h"
#include "V8Node.h"
#include "V8Notation.h"
#include "V8ProcessingInstruction.h"
#include "V8SVGElement.h"
#include "V8ShadowRoot.h"
#include "V8Text.h"
#include "bindings/v8/V8AbstractEventListener.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8EventListener.h"
#include "core/dom/Document.h"
#include "core/dom/EventListener.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "wtf/RefPtr.h"

namespace WebCore {

// This function is customized to take advantage of the optional 4th argument: AttachBehavior
void V8Node::insertBeforeMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Handle<v8::Object> holder = args.Holder();
    Node* imp = V8Node::toNative(holder);
    ExceptionCode ec = 0;
    Node* newChild = V8Node::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate())) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    Node* refChild = V8Node::HasInstance(args[1], args.GetIsolate(), worldType(args.GetIsolate())) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(args[1])) : 0;
    bool success = imp->insertBefore(newChild, refChild, ec, AttachLazily);
    if (ec) {
        setDOMException(ec, args.GetIsolate());
        return;
    }
    if (success) {
        v8SetReturnValue(args, args[0]);
        return;
    }
    v8SetReturnValueNull(args);
}

// This function is customized to take advantage of the optional 4th argument: AttachBehavior
void V8Node::replaceChildMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Handle<v8::Object> holder = args.Holder();
    Node* imp = V8Node::toNative(holder);
    ExceptionCode ec = 0;
    Node* newChild = V8Node::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate())) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    Node* oldChild = V8Node::HasInstance(args[1], args.GetIsolate(), worldType(args.GetIsolate())) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(args[1])) : 0;
    bool success = imp->replaceChild(newChild, oldChild, ec, AttachLazily);
    if (ec) {
        setDOMException(ec, args.GetIsolate());
        return;
    }
    if (success) {
        v8SetReturnValue(args, args[1]);
        return;
    }
    v8SetReturnValueNull(args);
}

void V8Node::removeChildMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Handle<v8::Object> holder = args.Holder();
    Node* imp = V8Node::toNative(holder);
    ExceptionCode ec = 0;
    Node* oldChild = V8Node::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate())) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    bool success = imp->removeChild(oldChild, ec);
    if (ec) {
        setDOMException(ec, args.GetIsolate());
        return;
    }
    if (success) {
        v8SetReturnValue(args, args[0]);
        return;
    }
    v8SetReturnValueNull(args);
}

// This function is customized to take advantage of the optional 4th argument: AttachBehavior
void V8Node::appendChildMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Handle<v8::Object> holder = args.Holder();
    Node* imp = V8Node::toNative(holder);
    ExceptionCode ec = 0;
    Node* newChild = V8Node::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate())) ? V8Node::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    bool success = imp->appendChild(newChild, ec, AttachLazily);
    if (ec) {
        setDOMException(ec, args.GetIsolate());
        return;
    }
    if (success) {
        v8SetReturnValue(args, args[0]);
        return;
    }
    v8SetReturnValueNull(args);
}

v8::Handle<v8::Object> wrap(Node* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    switch (impl->nodeType()) {
    case Node::ELEMENT_NODE:
        // For performance reasons, this is inlined from V8Element::wrap and must remain in sync.
        if (impl->isHTMLElement())
            return wrap(toHTMLElement(impl), creationContext, isolate);
        if (impl->isSVGElement())
            return wrap(toSVGElement(impl), creationContext, isolate);
        return V8Element::createWrapper(toElement(impl), creationContext, isolate);
    case Node::ATTRIBUTE_NODE:
        return wrap(toAttr(impl), creationContext, isolate);
    case Node::TEXT_NODE:
        return wrap(toText(impl), creationContext, isolate);
    case Node::CDATA_SECTION_NODE:
        return wrap(static_cast<CDATASection*>(impl), creationContext, isolate);
    case Node::ENTITY_NODE:
        return wrap(static_cast<Entity*>(impl), creationContext, isolate);
    case Node::PROCESSING_INSTRUCTION_NODE:
        return wrap(static_cast<ProcessingInstruction*>(impl), creationContext, isolate);
    case Node::COMMENT_NODE:
        return wrap(static_cast<Comment*>(impl), creationContext, isolate);
    case Node::DOCUMENT_NODE:
        return wrap(toDocument(impl), creationContext, isolate);
    case Node::DOCUMENT_TYPE_NODE:
        return wrap(static_cast<DocumentType*>(impl), creationContext, isolate);
    case Node::DOCUMENT_FRAGMENT_NODE:
        if (impl->isShadowRoot())
            return wrap(toShadowRoot(impl), creationContext, isolate);
        return wrap(static_cast<DocumentFragment*>(impl), creationContext, isolate);
    case Node::NOTATION_NODE:
        return wrap(static_cast<Notation*>(impl), creationContext, isolate);
    default:
        break; // ENTITY_REFERENCE_NODE or XPATH_NAMESPACE_NODE
    }
    return V8Node::createWrapper(impl, creationContext, isolate);
}
} // namespace WebCore
