#include <detours.h>

#include "skse64/GameData.h"
#include "skse64/GameForms.h"
#include "skse64/GameReferences.h"
#include "skse64/NiObjects.h"
#include "skse64/NiGeometry.h"
#include "skse64/NiExtraData.h"

#include "Hooks.h"
#include "HookEvents.h"
#include "Offsets.h"
#include "skse64/NiNodes.h"
#include "skse64/GameRTTI.h"
#include "skse64_common/SafeWrite.h"
#include <xbyak/xbyak.h>
#include "skse64_common/BranchTrampoline.h"
#include "ActorManager.h"

namespace hdt
{
	class BSFaceGenNiNodeEx : BSFaceGenNiNode
	{
		MEMBER_FN_PREFIX(BSFaceGenNiNodeEx);

	public:
		DEFINE_MEMBER_FN_HOOK(SkinAllGeometry, void, offset::BSFaceGenNiNode_SkinAllGeometry, NiNode* a_skeleton,
		                      char a_unk);
		DEFINE_MEMBER_FN_HOOK(SkinSingleGeometry, void, offset::BSFaceGenNiNode_SkinSingleGeometry, NiNode* a_skeleton,
		                      BSGeometry* a_geometry, char a_unk);

		void SkinAllGeometry(NiNode* a_skeleton, char a_unk)
		{
			// Logging
			const char* name = "";
			uint32_t formId = 0x0;

			if (a_skeleton->m_owner && a_skeleton->m_owner->baseForm)
			{
				auto bname = DYNAMIC_CAST(a_skeleton->m_owner->baseForm, TESForm, TESFullName);
				if (bname)
					name = bname->GetName();

				auto bnpc = DYNAMIC_CAST(a_skeleton->m_owner->baseForm, TESForm, TESNPC);

				if (bnpc && bnpc->nextTemplate)
					formId = bnpc->nextTemplate->formID;
			}

			_MESSAGE("SkinAllGeometry %s %d, %s, (formid %08x base form %08x head template form %08x)",
			         a_skeleton->m_name, a_skeleton->m_children.m_size, name,
			         a_skeleton->m_owner ? a_skeleton->m_owner->formID : 0x0,
			         a_skeleton->m_owner ? a_skeleton->m_owner->baseForm->formID : 0x0, formId);
			// End logging

			// Sending a ALL head geometry skinning event, and either call SkinAllGeometry or SkinAllGeometryCalls,
			// when we're processing the skeleton of the PC, or when we are asked to skin NPC face parts.
			if (ActorManager::instance()->m_skinNPCFaceParts || hdt::ActorManager::isNinodePlayerCharacter(a_skeleton))
			{
				SkinAllHeadGeometryEvent e;
				e.skeleton = a_skeleton;
				e.headNode = this;
				g_skinAllHeadGeometryEventDispatcher.dispatch(e);

				// The SkinAllGeometry function in the 1.6.x executables don't call all the required SkinSingleGeometry for the headparts,
				// as it does in the 1.5.97 executable.
#ifdef ANNIVERSARY_EDITION
				SkinAllGeometryCalls(a_skeleton, a_unk);
#else
				CALL_MEMBER_FN(this, SkinAllGeometry)(a_skeleton, a_unk);
#endif

				e.hasSkinned = true;
				g_skinAllHeadGeometryEventDispatcher.dispatch(e);
			}

			// Else we skin the skeleton. TODO What's the difference!?
			else
				CALL_MEMBER_FN(this, SkinAllGeometry)(a_skeleton, a_unk);
		}

		void SkinAllGeometryCalls(NiNode* a_skeleton, char a_unk)
		{
			// If this skeleton is the PC in 1st person, we skin it.
			if (ActorManager::isFirstPersonSkeleton(a_skeleton))
			{
				CALL_MEMBER_FN(this, SkinAllGeometry)(a_skeleton, a_unk);
				return;
			}

			TESForm* form = LookupFormByID(a_skeleton->m_owner->formID);
			Actor* actor = DYNAMIC_CAST(form, TESForm, Actor);

			// If we don't manage to cast the skeleton as an Actor, we skin it.
			if (!actor) {
				CALL_MEMBER_FN(this, SkinAllGeometry)(a_skeleton, a_unk);
				return;
			}

			TESNPC* actorBase = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
			auto actorBaseHasOverlays = CALL_MEMBER_FN(actorBase, HasOverlays)();
			UInt32 numHeadParts = actorBaseHasOverlays ? GetNumActorBaseOverlays(actorBase) : actorBase->numHeadParts;
			BGSHeadPart** Headparts = actorBaseHasOverlays ? GetActorBaseOverlays(actorBase) : actorBase->headparts;

			// We skin recursively all headparts and their extra parts
			if (Headparts)
				for (UInt32 i = 0; i < numHeadParts; i++)
					if (Headparts[i])
						ProcessHeadPart(Headparts[i], a_skeleton);

			// We skin anything that isn't the PC
			if (!hdt::ActorManager::isNinodePlayerCharacter(a_skeleton))
				CALL_MEMBER_FN(this, SkinAllGeometry)(a_skeleton, a_unk);
		}

		// Recursively SkinSingleGeometry of all headparts of a headpart
		void ProcessHeadPart(BGSHeadPart* headPart, NiNode* a_skeleton)
		{
			constexpr int skinSingleGeometryUnk = 20;

			if (!headPart)
				return;

			BSFixedString headPartName = headPart->partName;
			if (headPartName)
			{
				NiAVObject* headNode = this->GetObjectByName(&headPartName.data);
				if (headNode)
				{
					BSGeometry* headGeo = headNode->GetAsBSGeometry();
					if (headGeo)
						SkinSingleGeometry(a_skeleton, headGeo, skinSingleGeometryUnk);
				}
			}

			BGSHeadPart* extraPart = NULL;
			for (UInt32 p = 0; p < headPart->extraParts.count; p++)
			{
				if (headPart->extraParts.GetNthItem(p, extraPart))
					ProcessHeadPart(extraPart, a_skeleton);
			}
		}

		void SkinSingleGeometry(NiNode* a_skeleton, BSGeometry* a_geometry, char a_unk)
		{
			// Logging
			const char* name = "";
			uint32_t formId = 0x0;

			if (a_skeleton->m_owner && a_skeleton->m_owner->baseForm)
			{
				auto bname = DYNAMIC_CAST(a_skeleton->m_owner->baseForm, TESForm, TESFullName);
				if (bname)
					name = bname->GetName();

				auto bnpc = DYNAMIC_CAST(a_skeleton->m_owner->baseForm, TESForm, TESNPC);

				if (bnpc && bnpc->nextTemplate)
					formId = bnpc->nextTemplate->formID;
			}

			_MESSAGE("SkinSingleGeometry %s %d - %s, %s, (formid %08x base form %08x head template form %08x)",
				a_skeleton->m_name, a_skeleton->m_children.m_size, a_geometry->m_name, name,
				a_skeleton->m_owner ? a_skeleton->m_owner->formID : 0x0,
				a_skeleton->m_owner ? a_skeleton->m_owner->baseForm->formID : 0x0, formId);
			// End Logging

			// Sending a SINGLE head geometry skinning event when we're processing the skeleton of the PC, or when we are asked to skin NPC face parts.
			if (ActorManager::instance()->m_skinNPCFaceParts || hdt::ActorManager::isNinodePlayerCharacter(a_skeleton))
			{
				SkinSingleHeadGeometryEvent e;
				e.skeleton = a_skeleton;
				e.geometry = a_geometry;
				e.headNode = this;
				g_skinSingleHeadGeometryEventDispatcher.dispatch(e);
			}

			// Else we skin the skeleton. TODO What's the difference!?
			else
				CALL_MEMBER_FN(this, SkinSingleGeometry)(a_skeleton, a_geometry, a_unk);
		}
	};

	RelocAddr<uintptr_t> BoneLimit(offset::BSFaceGenModelExtraData_BoneLimit);
	
	void hookFaceGen()
	{
		DetourAttach((void**)BSFaceGenNiNodeEx::_SkinSingleGeometry_GetPtrAddr(),
			(void*)GetFnAddr(&BSFaceGenNiNodeEx::SkinSingleGeometry));
		DetourAttach((void**)BSFaceGenNiNodeEx::_SkinAllGeometry_GetPtrAddr(),
			(void*)GetFnAddr(&BSFaceGenNiNodeEx::SkinAllGeometry));

		RelocAddr<uintptr_t> addr(offset::BSFaceGenNiNode_SkinSingleGeometry_bug);
		SafeWrite8(addr.GetUIntPtr(), 0x7);

#ifndef ANNIVERSARY_EDITION
		struct BSFaceGenExtraModelData_BoneCount_Code : Xbyak::CodeGenerator
		{
			BSFaceGenExtraModelData_BoneCount_Code(void* buf) : CodeGenerator(4096, buf)
			{
				Xbyak::Label j_Out;

				mov(esi, ptr[rax + 0x58]);
				cmp(esi, 9);
				jl(j_Out);
				mov(esi, 8);
				L(j_Out);
				jmp(ptr[rip]);
				dq(BoneLimit.GetUIntPtr() + 0x7);
			}
		};

		void* codeBuf = g_localTrampoline.StartAlloc();
		BSFaceGenExtraModelData_BoneCount_Code code(codeBuf);
		g_localTrampoline.EndAlloc(code.getCurr());

		g_branchTrampoline.Write5Branch(BoneLimit.GetUIntPtr(), uintptr_t(code.getCode()));
#endif // !ANNIVERSARY_EDITION

	}

	void unhookFaceGen()
	{
		DetourDetach((void**)BSFaceGenNiNodeEx::_SkinSingleGeometry_GetPtrAddr(),
		             (void*)GetFnAddr(&BSFaceGenNiNodeEx::SkinSingleGeometry));
		DetourDetach((void**)BSFaceGenNiNodeEx::_SkinAllGeometry_GetPtrAddr(),
		             (void*)GetFnAddr(&BSFaceGenNiNodeEx::SkinAllGeometry));
	}

	struct Unk001CB0E0
	{
		MEMBER_FN_PREFIX(Unk001CB0E0);

		DEFINE_MEMBER_FN_HOOK(unk001CB0E0, NiAVObject*, offset::ArmorAttachFunction, NiNode* armor, NiNode* skeleton,
		                      void* unk3, char unk4, char unk5, void* unk6);

		NiAVObject* unk001CB0E0(NiNode* armor, NiNode* skeleton, void* unk3, char unk4, char unk5, void* unk6)
		{
			ArmorAttachEvent event;
			event.armorModel = armor;
			event.skeleton = skeleton;
			g_armorAttachEventDispatcher.dispatch(event);

			auto ret = CALL_MEMBER_FN(this, unk001CB0E0)(armor, skeleton, unk3, unk4, unk5, unk6);

			if (ret) {
			event.attachedNode = ret;
			event.hasAttached = true;
			g_armorAttachEventDispatcher.dispatch(event);
			}
			return ret;
		}
	};

	void hookArmor()
	{
		DetourAttach((void**)Unk001CB0E0::_unk001CB0E0_GetPtrAddr(), (void*)GetFnAddr(&Unk001CB0E0::unk001CB0E0));
	}

	void unhookArmor()
	{
		DetourDetach((void**)Unk001CB0E0::_unk001CB0E0_GetPtrAddr(), (void*)GetFnAddr(&Unk001CB0E0::unk001CB0E0));
	}

	struct UnkEngine
	{
		MEMBER_FN_PREFIX(UnkEngine);

		DEFINE_MEMBER_FN_HOOK(onFrame, void, offset::GameLoopFunction);

		void onFrame();

		char unk[0x16];
		bool gamePaused; // 16
	};

	void UnkEngine::onFrame()
	{
		CALL_MEMBER_FN(this, onFrame)();
		FrameEvent e;
		e.gamePaused = this->gamePaused;
		g_frameEventDispatcher.dispatch(e);
	}

	auto oldShutdown = (void (*)(bool))(RelocationManager::s_baseAddr + offset::GameShutdownFunction);

	void shutdown(bool arg0)
	{
		g_shutdownEventDispatcher.dispatch(ShutdownEvent());
		oldShutdown(arg0);
	}

	void hookEngine()
	{
		DetourAttach((void**)UnkEngine::_onFrame_GetPtrAddr(), (void*)GetFnAddr(&UnkEngine::onFrame));
		DetourAttach((void**)&oldShutdown, static_cast<void*>(shutdown));
	}

	void unhookEngine()
	{
		DetourDetach((void**)UnkEngine::_onFrame_GetPtrAddr(), (void*)GetFnAddr(&UnkEngine::onFrame));
		DetourDetach((void**)&oldShutdown, static_cast<void*>(shutdown));
	}

	void hookAll()
	{
		DetourRestoreAfterWith();
		DetourTransactionBegin();
		hookEngine();
		hookArmor();
		hookFaceGen();
		DetourTransactionCommit();
	}

	void unhookAll()
	{
		DetourRestoreAfterWith();
		DetourTransactionBegin();
		unhookEngine();
		unhookArmor();
		unhookFaceGen();
		DetourTransactionCommit();
	}
}
