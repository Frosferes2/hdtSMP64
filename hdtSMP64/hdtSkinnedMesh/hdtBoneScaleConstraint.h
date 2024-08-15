#pragma once

#include "hdtSkinnedMeshBody.h"

namespace hdt
{
	class BoneScaleConstraint : public RefObject
	{
	public:
		BoneScaleConstraint(SkinnedMeshBone* a, SkinnedMeshBone* b, std::function<void(const btTransform&, const btQsTransform&, const btQsTransform&, btTransform&, btTransform&)> f, btTransform frame, btTypedConstraint* constraint);
		~BoneScaleConstraint();

		virtual void scaleConstraint() = 0;
		virtual void updateFrame() = 0;
		std::function<void(const btTransform&, const btQsTransform&, const btQsTransform&, btTransform&, btTransform&)> calcFrame;
		void applyCalcFrame(std::function<void(const btTransform&, const btQsTransform&, const btQsTransform&, btTransform&, btTransform&)>, btTransform&, btTransform&);
		
		btTypedConstraint* getConstraint() const { return m_constraint; }

		float m_scaleA, m_scaleB;

		SkinnedMeshBone* m_boneA;
		SkinnedMeshBone* m_boneB;
		btTransform m_frame;
		btTypedConstraint* m_constraint;
	};
}
