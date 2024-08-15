#pragma once
#include "hdtBoneScaleConstraint.h"

namespace hdt
{
	class ConeTwistConstraint
		: public BoneScaleConstraint
		  , public btConeTwistConstraint
	{
	public:

		ConeTwistConstraint(SkinnedMeshBone* a, SkinnedMeshBone* b, const std::function<void(const btTransform&, const btQsTransform&, const btQsTransform&, btTransform&, btTransform&)> func,
			                const btTransform& frame);

		void scaleConstraint() override;
		void updateFrame() override;
	};
}
