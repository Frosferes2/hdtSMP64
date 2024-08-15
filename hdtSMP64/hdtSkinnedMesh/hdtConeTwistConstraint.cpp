#include "hdtConeTwistConstraint.h"

namespace hdt
{
	ConeTwistConstraint::ConeTwistConstraint(SkinnedMeshBone* a, SkinnedMeshBone* b, const std::function<void(const btTransform&, const btQsTransform&, const btQsTransform&, btTransform&, btTransform&)> func,
	                                         const btTransform& frame)
		: BoneScaleConstraint(a, b, func, frame, static_cast<btConeTwistConstraint*>(this))
		  , btConeTwistConstraint(a->m_rig, b->m_rig, btTransform::getIdentity(), btTransform::getIdentity())
	{
		btTransform frameInA, frameInB;
		applyCalcFrame(calcFrame, frameInA, frameInB);
		btTransform fa = m_boneA->m_rigToLocal * frameInA;
		btTransform fb = m_boneB->m_rigToLocal * frameInB;
		setFrames(fa, fb);

		enableMotor(false); // motor temporary not support
	}

	void ConeTwistConstraint::scaleConstraint()
	{
		auto newScaleA = m_boneA->m_currentTransform.getScale();
		auto newScaleB = m_boneB->m_currentTransform.getScale();

		if (btFuzzyZero(newScaleA - m_scaleA) && btFuzzyZero(newScaleB - m_scaleB))
			return;

		auto frameA = getFrameOffsetA();
		auto frameB = getFrameOffsetB();
		frameA.setOrigin(frameA.getOrigin() * (newScaleA / m_scaleA));
		frameB.setOrigin(frameB.getOrigin() * (newScaleB / m_scaleB));
		setFrames(frameA, frameB);
		m_scaleA = newScaleA;
		m_scaleB = newScaleB;
	}

	void ConeTwistConstraint::updateFrame()
	{
		btTransform frameInA, frameInB;
		applyCalcFrame(calcFrame, frameInA, frameInB);
		btTransform fa = m_boneA->m_rigToLocal * frameInA;
		btTransform fb = m_boneB->m_rigToLocal * frameInB;
		setFrames(fa, fb);
	}
}