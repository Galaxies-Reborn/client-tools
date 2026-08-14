#include "_precompile.h"

#include "UICanvas.h"
#include "UICanvasGenerator.h"
#include "UIDeformer.h"
#include "UIUtils.h"

#include <cassert>
#include <cmath>
#include <stack>
#include <vector>

//----------------------------------------------------------------------

const char *UICanvas::TypeName = "Canvas";

namespace
{
	static UIFloatPoint s_theVertices[4];
	static UIFloatPoint s_theUVs[4];

	void getTransformedBounds(UICanvas const & canvas, float const left, float const top, float const right, float const bottom, UIRect & bounds)
	{
		UIFloatPoint const points[4] =
		{
			canvas.TransformFP(left, top),
			canvas.TransformFP(right, top),
			canvas.TransformFP(left, bottom),
			canvas.TransformFP(right, bottom)
		};

		float minimumX = points[0].x;
		float minimumY = points[0].y;
		float maximumX = points[0].x;
		float maximumY = points[0].y;
		for (int index = 1; index != 4; ++index)
		{
			minimumX = std::min(minimumX, points[index].x);
			minimumY = std::min(minimumY, points[index].y);
			maximumX = std::max(maximumX, points[index].x);
			maximumY = std::max(maximumY, points[index].y);
		}

		bounds.left = static_cast<long>(std::floor(minimumX));
		bounds.top = static_cast<long>(std::floor(minimumY));
		bounds.right = static_cast<long>(std::ceil(maximumX));
		bounds.bottom = static_cast<long>(std::ceil(maximumY));
	}

	UIFloatPoint interpolateQuad(UIFloatPoint const source[4], float const horizontalAmount, float const verticalAmount)
	{
		UIFloatPoint const top(
			source[0].x + (source[1].x - source[0].x) * horizontalAmount,
			source[0].y + (source[1].y - source[0].y) * horizontalAmount);
		UIFloatPoint const bottom(
			source[2].x + (source[3].x - source[2].x) * horizontalAmount,
			source[2].y + (source[3].y - source[2].y) * horizontalAmount);

		return UIFloatPoint(
			top.x + (bottom.x - top.x) * verticalAmount,
			top.y + (bottom.y - top.y) * verticalAmount);
	}

	bool clipAxisAlignedQuad(UIFloatPoint vertices[4], UIFloatPoint * const textureCoordinates, UIRect const & clippingRect)
	{
		float const epsilon = 0.001f;
		if (std::fabs(vertices[0].y - vertices[1].y) > epsilon ||
			std::fabs(vertices[2].y - vertices[3].y) > epsilon ||
			std::fabs(vertices[0].x - vertices[2].x) > epsilon ||
			std::fabs(vertices[1].x - vertices[3].x) > epsilon)
		{
			return true;
		}

		float const originalLeft = vertices[0].x;
		float const originalTop = vertices[0].y;
		float const originalRight = vertices[1].x;
		float const originalBottom = vertices[2].y;
		if (originalRight <= originalLeft || originalBottom <= originalTop)
			return true;

		float const clippedLeft = std::max(originalLeft, static_cast<float>(clippingRect.left));
		float const clippedTop = std::max(originalTop, static_cast<float>(clippingRect.top));
		float const clippedRight = std::min(originalRight, static_cast<float>(clippingRect.right));
		float const clippedBottom = std::min(originalBottom, static_cast<float>(clippingRect.bottom));
		if (clippedRight <= clippedLeft || clippedBottom <= clippedTop)
			return false;

		float const inverseWidth = 1.0f / (originalRight - originalLeft);
		float const inverseHeight = 1.0f / (originalBottom - originalTop);
		float const leftAmount = (clippedLeft - originalLeft) * inverseWidth;
		float const topAmount = (clippedTop - originalTop) * inverseHeight;
		float const rightAmount = (clippedRight - originalLeft) * inverseWidth;
		float const bottomAmount = (clippedBottom - originalTop) * inverseHeight;

		vertices[0] = UIFloatPoint(clippedLeft, clippedTop);
		vertices[1] = UIFloatPoint(clippedRight, clippedTop);
		vertices[2] = UIFloatPoint(clippedLeft, clippedBottom);
		vertices[3] = UIFloatPoint(clippedRight, clippedBottom);

		if (textureCoordinates)
		{
			UIFloatPoint const originalTextureCoordinates[4] =
			{
				textureCoordinates[0],
				textureCoordinates[1],
				textureCoordinates[2],
				textureCoordinates[3]
			};

			textureCoordinates[0] = interpolateQuad(originalTextureCoordinates, leftAmount, topAmount);
			textureCoordinates[1] = interpolateQuad(originalTextureCoordinates, rightAmount, topAmount);
			textureCoordinates[2] = interpolateQuad(originalTextureCoordinates, leftAmount, bottomAmount);
			textureCoordinates[3] = interpolateQuad(originalTextureCoordinates, rightAmount, bottomAmount);
		}

		return true;
	}
}

//----------------------------------------------------------------------

UICanvas::UICanvas( const UISize &Size ) :
UIBaseObject(),
mSize       (Size),
mOrigin     (0, 0),
mSrcScale   (1.0f, 1.0f),
mGenerator  (0),
mState      (),
mStateStack (0),
mFlushCount (0)
{
	mState.TranslationOnly = true;
	mState.EnableFiltering = false;

	mState.Translation.x = 0;
	mState.Translation.y = 0;

	mState.mShear.x = 0.0f;
	mState.mShear.y = 0.0f;
	
	mState.TransformationMatrix[0][0] = 1.0f;
	mState.TransformationMatrix[0][1] = 0.0f;
	mState.TransformationMatrix[0][2] = 0.0f;
	mState.TransformationMatrix[1][0] = 0.0f;
	mState.TransformationMatrix[1][1] = 1.0f;
	mState.TransformationMatrix[1][2] = 0.0f;
	mState.TransformationMatrix[2][0] = 0.0f;
	mState.TransformationMatrix[2][1] = 0.0f;
	mState.TransformationMatrix[2][2] = 1.0f;

	// Initial clipping rectangle is not the maximimum size because then
	// translation / scaling, etc on an unclipped world would not work
	mState.ClippingRect.top    = -(INT_MAX / 1000);
	mState.ClippingRect.left   = -(INT_MAX / 1000);
	mState.ClippingRect.bottom =  (INT_MAX / 1000);
	mState.ClippingRect.right  =  (INT_MAX / 1000);

	// Base color is opaque white
	mState.Color.Set( 0xFF, 0xFF, 0xFF, 0xFF );
	mState.Opacity = 1.0f;

	mState.mDepth = 1.0f;
	mState.DepthOverride = false;
}

//----------------------------------------------------------------------

UICanvas::~UICanvas()
{
	SetGenerator(0);

	mGenerator = NULL;

#ifdef _DEBUG
	if (mStateStack)
	{
		assert(mStateStack->empty()); //lint !e1924 // C-style cast MSVC bug
	}
#endif

	delete mStateStack;
	mStateStack = 0;

}

//----------------------------------------------------------------------

bool UICanvas::IsA( const UITypeID Type ) const
{
	return (Type == TUICanvas) || UIBaseObject::IsA( Type );
}

//----------------------------------------------------------------------

const char *UICanvas::GetTypeName() const
{
	return TypeName;
}

//----------------------------------------------------------------------

void UICanvas::SetGenerator( UICanvasGenerator *NewGenerator )
{
	if( NewGenerator )
		NewGenerator->Attach( this );

	if( mGenerator )
		mGenerator->Detach( this );

	mGenerator = NewGenerator;
}

//----------------------------------------------------------------------

void UICanvas::GetGenerator( UICanvasGenerator *&Out ) const
{
	Out = mGenerator;
}

//----------------------------------------------------------------------

bool UICanvas::Generate() const
{
	return !mGenerator || mGenerator->GenerateOnto( *const_cast<UICanvas *>( this ) );
}

//----------------------------------------------------------------------

void UICanvas::SetSize( const UISize &NewSize )
{
	mSize = NewSize;
}

//----------------------------------------------------------------------

void UICanvas::GetSize( UISize &OutSize ) const
{
	OutSize = mSize;
}

//----------------------------------------------------------------------

long UICanvas::GetWidth() const
{
	return mSize.x;
}

//----------------------------------------------------------------------

long UICanvas::GetHeight() const
{
	return mSize.y;
}

//----------------------------------------------------------------------

void UICanvas::BltFrom( const UICanvas * const src, const UIPoint &Source, const UIPoint &Destination, const UISize &Size )
{
	UIPoint MappedDestination = Destination + mState.Translation;
	UIRect	DestRect;

	DestRect.top		= MappedDestination.y;
	DestRect.left		= MappedDestination.x;
	DestRect.bottom	    = MappedDestination.y + Size.y;
	DestRect.right	    = MappedDestination.x + Size.x;

	UIPoint clippedSourcePt (Source);

	if( mState.TranslationOnly )
	{
		const UIRect oldDestRect (DestRect);

		if( !UIUtils::ClipRect( DestRect, mState.ClippingRect ) )
			return;

		if (DestRect.left > oldDestRect.left)
			clippedSourcePt.x += static_cast<long> (static_cast<float> (DestRect.left - oldDestRect.left) * mSrcScale.x);

		if (DestRect.top > oldDestRect.top)
			clippedSourcePt.y += static_cast<long> (static_cast<float> (DestRect.top - oldDestRect.top) * mSrcScale.y);

	}

	UIRect SourceRect;

	SourceRect.top		= Source.y        + static_cast<long> (static_cast<float> (DestRect.top  - MappedDestination.y) * mSrcScale.y);
	SourceRect.left		= Source.x        + static_cast<long> (static_cast<float> (DestRect.left - MappedDestination.x) * mSrcScale.x);
	SourceRect.bottom	= SourceRect.top  + DestRect.Height ();
	SourceRect.right	= SourceRect.left + DestRect.Width ();

	// Copy data to vertex buffer, offseting geometry
	// so texture centers lie on pixel centers	

	DestRect -= mState.Translation;
	
	s_theVertices[0] = TransformFP( static_cast<float>(DestRect.left),  static_cast<float>(DestRect.top) );
	s_theVertices[1] = TransformFP( static_cast<float>(DestRect.right), static_cast<float>(DestRect.top) );	
	s_theVertices[2] = TransformFP( static_cast<float>(DestRect.left),  static_cast<float>(DestRect.bottom) );
	s_theVertices[3] = TransformFP( static_cast<float>(DestRect.right), static_cast<float>(DestRect.bottom) );

	if( src )
	{
		const float InverseSourceWidth  = 1.0f / static_cast<float>(src->GetWidth());
		const float InverseSourceHeight = 1.0f / static_cast<float>(src->GetHeight());

		const UIFloatPoint srcMin (static_cast<float>(SourceRect.left), static_cast<float>(SourceRect.top));
		const UIFloatPoint srcMax (static_cast<float>(SourceRect.right), static_cast<float>(SourceRect.bottom));

		s_theUVs[0].x	= srcMin.x * InverseSourceWidth;
		s_theUVs[0].y	= srcMin.y * InverseSourceHeight;
		s_theUVs[1].x	= (srcMin.x + (srcMax.x - srcMin.x) * mSrcScale.x) * InverseSourceWidth;
		s_theUVs[1].y	= s_theUVs[0].y;
		s_theUVs[2].x	= s_theUVs[0].x;
		s_theUVs[2].y	= (srcMin.y + (srcMax.y - srcMin.y) * mSrcScale.y) * InverseSourceHeight;
		s_theUVs[3].x	= s_theUVs[1].x;
		s_theUVs[3].y	= s_theUVs[2].y;
	}

	if (!mState.TranslationOnly && !clipAxisAlignedQuad(s_theVertices, src ? s_theUVs : 0, mState.ClippingRect))
		return;

	RenderQuad( src, s_theVertices, s_theUVs );
}

//----------------------------------------------------------------------

void UICanvas::RenderLine(const UILine & line)
{
	// For now, do not draw these lines when deforming.
	if (IsDeforming()) 
	{
		UIFloatPoint deformScale;
		GetDeformScale(deformScale);
		
		if (deformScale != UIFloatPoint::one) 
		{
			return;
		}
	}

	RenderLines (0, 1, &line, 0);
}

//----------------------------------------------------------------------

void UICanvas::RenderLines      (const UICanvas * const , int ,   const UILine * , const UILine * )
{		
}

//----------------------------------------------------------------------

void UICanvas::RenderLines(const UICanvas * const , int , const UILine * , const UIFloatPoint * , const UIColor *)
{		
}

//----------------------------------------------------------------------

void UICanvas::RenderLineStrip(const UICanvas * const ,  int, const UIFloatPoint * )
{
}

//----------------------------------------------------------------------

void UICanvas::RenderTriangles      (const UICanvas * const , int , const UITriangle * , const UITriangle * )
{
}

//----------------------------------------------------------------------

void UICanvas::Refresh()
{
}

//----------------------------------------------------------------------

void UICanvas::RenderQuad( const UICanvas * const, const UIFloatPoint [4], const UIFloatPoint [4] )
{
}

//----------------------------------------------------------------------

void UICanvas::RenderQuad( const UICanvas * const, const UIFloatPoint [4], const UIFloatPoint [4], const UIColor [4] )
{
}

//----------------------------------------------------------------------

void UICanvas::Rotate( float theta )
{
	theta *= 6.283185307179586476925286766559f;

	float sTheta = sinf( theta );
	float cTheta = cosf( theta );

	// Matrix layout:
	//   a b c
	//   d e f
	//   g h i

	float a = mState.TransformationMatrix[0][0];
	float b = mState.TransformationMatrix[0][1];
	float d = mState.TransformationMatrix[1][0];
	float e = mState.TransformationMatrix[1][1];
	float g = mState.TransformationMatrix[2][0];
	float h = mState.TransformationMatrix[2][1];

	mState.TransformationMatrix[0][0] =  a * cTheta + b * sTheta;
	mState.TransformationMatrix[0][1] = -a * sTheta + b * cTheta;
	
	mState.TransformationMatrix[1][0] =  d * cTheta + e * sTheta;
	mState.TransformationMatrix[1][1] = -d * sTheta + e * cTheta;
	
	mState.TransformationMatrix[2][0] =  g * cTheta + h * sTheta;
	mState.TransformationMatrix[2][1] = -g * sTheta + h * cTheta;

	mState.TranslationOnly = false;
}

//----------------------------------------------------------------------

void UICanvas::PushState( void )
{
	if (!mStateStack)
	{
		mStateStack = new UICanvasStateStack;
	}

	mStateStack->push( mState );
}

//----------------------------------------------------------------------

void UICanvas::RestoreState ()
{
	assert(mStateStack);
	assert(!mStateStack->empty()); //lint !e1924 // C-style cast MSVC bug

	UICanvasState &TopElement = mStateStack->top();

	if( mState.EnableFiltering != TopElement.EnableFiltering )
		EnableFiltering( TopElement.EnableFiltering );

	mState = TopElement;
}

//----------------------------------------------------------------------

void UICanvas::RestoreClip ()
{
	assert(mStateStack);
	assert(!mStateStack->empty()); //lint !e1924 // C-style cast MSVC bug

	mState.ClippingRect = mStateStack->top().ClippingRect;
}

//----------------------------------------------------------------------

void UICanvas::RestoreTransform ()
{
	assert(mStateStack);
	assert(!mStateStack->empty()); //lint !e1924 // C-style cast MSVC bug

	UICanvasState &TopElement = mStateStack->top();

	mState.Translation = TopElement.Translation;
	memcpy( mState.TransformationMatrix, TopElement.TransformationMatrix, sizeof( mState.TransformationMatrix ) );
}

//----------------------------------------------------------------------

void UICanvas::RestoreOpacity ()
{
	assert(mStateStack);
	assert(!mStateStack->empty()); //lint !e1924 // C-style cast MSVC bug

	SetOpacity( mStateStack->top().Opacity );
}

//----------------------------------------------------------------------

void UICanvas::PopState ()
{
	assert(mStateStack);
	assert(!mStateStack->empty()); //lint !e1924 // C-style cast MSVC bug

	UICanvasState &TopElement = mStateStack->top();

	if( mState.EnableFiltering != TopElement.EnableFiltering )
		EnableFiltering( TopElement.EnableFiltering );

	mState = TopElement;
	mStateStack->pop();
}

//----------------------------------------------------------------------

bool UICanvas::Clip( UIScalar left, UIScalar top, UIScalar right, UIScalar bottom )
{
//	PerformShearing (left,  top,    mState.mShear);
//	PerformShearing (right, bottom, mState.mShear);
	if (!mState.TranslationOnly)
	{
		UIRect transformedBounds;
		getTransformedBounds(*this, static_cast<float>(left), static_cast<float>(top), static_cast<float>(right), static_cast<float>(bottom), transformedBounds);
		return UIUtils::ClipRect(mState.ClippingRect, transformedBounds);
	}

	return UIUtils::ClipRect( mState.ClippingRect, 
		left   + mState.Translation.x,
		top    + mState.Translation.y,
		right  + mState.Translation.x,
		bottom + mState.Translation.y);
}

//----------------------------------------------------------------------

void UICanvas::SetClip(UIRect const & in)
{
	if (mState.TranslationOnly)
	{
		mState.ClippingRect.top = in.top + mState.Translation.y;
		mState.ClippingRect.left = in.left + mState.Translation.x;
		mState.ClippingRect.bottom = in.bottom + mState.Translation.y;
		mState.ClippingRect.right = in.right + mState.Translation.x;
		return;
	}

	getTransformedBounds(*this,
		static_cast<float>(in.left),
		static_cast<float>(in.top),
		static_cast<float>(in.right),
		static_cast<float>(in.bottom),
		mState.ClippingRect);
}

//----------------------------------------------------------------------

void UICanvas::GetClip(UIRect & out) const
{
	if (mState.TranslationOnly)
	{
		out.top = mState.ClippingRect.top - mState.Translation.y;
		out.left = mState.ClippingRect.left - mState.Translation.x;
		out.bottom = mState.ClippingRect.bottom - mState.Translation.y;
		out.right = mState.ClippingRect.right - mState.Translation.x;
		return;
	}

	float const shearX = mState.mShear.x;
	float const shearY = mState.mShear.y;
	float const a = mState.TransformationMatrix[0][0] + mState.TransformationMatrix[0][1] * shearY;
	float const b = mState.TransformationMatrix[0][0] * shearX + mState.TransformationMatrix[0][1];
	float const c = mState.TransformationMatrix[0][2];
	float const d = mState.TransformationMatrix[1][0] + mState.TransformationMatrix[1][1] * shearY;
	float const e = mState.TransformationMatrix[1][0] * shearX + mState.TransformationMatrix[1][1];
	float const f = mState.TransformationMatrix[1][2];
	float const determinant = a * e - b * d;

	if (std::fabs(determinant) < 0.000001f)
	{
		out = mState.ClippingRect;
		return;
	}

	float const inverseDeterminant = 1.0f / determinant;
	UIFloatPoint points[4];
	UIRect const & clip = mState.ClippingRect;
	float const physicalX[4] = { static_cast<float>(clip.left), static_cast<float>(clip.right), static_cast<float>(clip.left), static_cast<float>(clip.right) };
	float const physicalY[4] = { static_cast<float>(clip.top), static_cast<float>(clip.top), static_cast<float>(clip.bottom), static_cast<float>(clip.bottom) };
	for (int index = 0; index != 4; ++index)
	{
		float const translatedX = physicalX[index] - c;
		float const translatedY = physicalY[index] - f;
		points[index].x = (e * translatedX - b * translatedY) * inverseDeterminant;
		points[index].y = (-d * translatedX + a * translatedY) * inverseDeterminant;
	}

	float minimumX = points[0].x;
	float minimumY = points[0].y;
	float maximumX = points[0].x;
	float maximumY = points[0].y;
	for (int index = 1; index != 4; ++index)
	{
		minimumX = std::min(minimumX, points[index].x);
		minimumY = std::min(minimumY, points[index].y);
		maximumX = std::max(maximumX, points[index].x);
		maximumY = std::max(maximumY, points[index].y);
	}

	out.left = static_cast<long>(std::floor(minimumX));
	out.top = static_cast<long>(std::floor(minimumY));
	out.right = static_cast<long>(std::ceil(maximumX));
	out.bottom = static_cast<long>(std::ceil(maximumY));
}

//----------------------------------------------------------------------

bool UICanvas::BeginRendering ()
{
	mFlushCount = 0; 
	return true;
}

//----------------------------------------------------------------------

void UICanvas::EndRendering () {}

//----------------------------------------------------------------------

void UICanvas::Flush ()
{
	++mFlushCount;
}

//----------------------------------------------------------------------

bool UICanvas::IsDeforming() const
{
	return !mState.mDeformers.empty();
}

//----------------------------------------------------------------------

void UICanvas::QueueDeformer(UIDeformer & deformer)
{
	mState.mDeformers.push_back(UIWatcher<UIDeformer>(&deformer));
}

//----------------------------------------------------------------------

void UICanvas::DequeueDeformer(UIDeformer const & deformer)
{
	UI_ASSERT(!mState.mDeformers.empty());
	UI_ASSERT(mState.mDeformers.back() == &deformer);
	mState.mDeformers.pop_back();
}

//----------------------------------------------------------------------

void UICanvas::Deform(UIFloatPoint const * points, UIFloatPoint * out, size_t const count)
{
	UIFloatPoint const * sourcePointer = points;

	for (UICanvasState::DeformerVector::iterator itDeformer = mState.mDeformers.begin(); itDeformer != mState.mDeformers.end(); ++itDeformer)
	{
		UIDeformer * const deformer = *itDeformer;
		if (deformer)
		{
			if (deformer->Deform(*this, sourcePointer, out, count))
			{
				sourcePointer = out;
			}
		}
	}
}

//----------------------------------------------------------------------

void UICanvas::GetDeformScale(UIFloatPoint & DeformScale)
{
	DeformScale = UIFloatPoint::one;

	for (UICanvasState::DeformerVector::const_iterator itDeformer = mState.mDeformers.begin(); itDeformer != mState.mDeformers.end(); ++itDeformer)
	{
		UIDeformer const * const deformer = *itDeformer;
		if (deformer)
		{
			DeformScale *= deformer->GetDeformedScale();
		}
	}
}

//----------------------------------------------------------------------

void UICanvas::BltFromNoScaleOrRotate( const UICanvas * const src, const UIPoint &Source, const UIPoint &Destination, const UISize &Size )
{
	UIPoint MappedDestination = Destination + mState.Translation;
	UIRect	DestRect;

	DestRect.top    = MappedDestination.y;
	DestRect.left   = MappedDestination.x;
	DestRect.bottom	= MappedDestination.y + Size.y;
	DestRect.right	= MappedDestination.x + Size.x;

	UIPoint clippedSourcePt (Source);

	if( mState.TranslationOnly )
	{
		const UIRect oldDestRect (DestRect);

		if( !UIUtils::ClipRect( DestRect, mState.ClippingRect ) )
			return;

		if (DestRect.left > oldDestRect.left)
			clippedSourcePt.x += static_cast<long> (static_cast<float> (DestRect.left - oldDestRect.left) * mSrcScale.x);

		if (DestRect.top > oldDestRect.top)
			clippedSourcePt.y += static_cast<long> (static_cast<float> (DestRect.top - oldDestRect.top) * mSrcScale.y);

	}

	// Ultrawide UI-scale support (2026-05-16): when a canvas Scale is active
	// (TranslationOnly == false), this "NoScaleOrRotate" fast path was
	// silently ignoring the scale matrix -- vertices written from raw
	// DestRect coords -- so text glyphs (only caller is UITextStyle.cpp
	// per-glyph blits, lines 985/1001/1021) rendered at original pixel
	// size while widget panels (using regular BltFrom, which DOES apply
	// the matrix) rendered scaled. Fix: when TranslationOnly is false,
	// rebase DestRect to widget-local coords (subtract raw Translation
	// just like BltFrom does at its line ~197) and apply the full
	// transformation matrix via TransformFP. Fast path preserved when
	// TranslationOnly is true (normal scale-1.0 case) -- zero overhead
	// for default UI scale.
	if (!mState.TranslationOnly)
	{
		DestRect -= mState.Translation;
		s_theVertices[0] = TransformFP(static_cast<float>(DestRect.left),  static_cast<float>(DestRect.top));
		s_theVertices[1] = TransformFP(static_cast<float>(DestRect.right), static_cast<float>(DestRect.top));
		s_theVertices[2] = TransformFP(static_cast<float>(DestRect.left),  static_cast<float>(DestRect.bottom));
		s_theVertices[3] = TransformFP(static_cast<float>(DestRect.right), static_cast<float>(DestRect.bottom));
	}
	else
	{
		s_theVertices[0].x = (float)DestRect.left;
		s_theVertices[0].y = (float)DestRect.top;
		s_theVertices[1].x = (float)DestRect.right;
		s_theVertices[1].y = s_theVertices[0].y;
		s_theVertices[2].x = s_theVertices[0].x;
		s_theVertices[2].y = (float)DestRect.bottom;
		s_theVertices[3].x = s_theVertices[1].x;
		s_theVertices[3].y = s_theVertices[2].y;
	}

	if ( src )
	{
		const float invSrcWidth = 1.0f / (float)src->GetWidth();
		const float invSrcHeight = 1.0f / (float)src->GetHeight();

		UIRect SourceRect;

		SourceRect.top		= clippedSourcePt.y;
		SourceRect.left		= clippedSourcePt.x;
		SourceRect.bottom	= SourceRect.top + DestRect.Height ();
		SourceRect.right	= SourceRect.left + DestRect.Width ();


		s_theUVs[0].x = (float)SourceRect.left * invSrcWidth;
		s_theUVs[0].y = (float)SourceRect.top * invSrcHeight;

		s_theUVs[1].x = (float)SourceRect.right * invSrcWidth;
		s_theUVs[1].y = s_theUVs[0].y;

		s_theUVs[2].x = s_theUVs[0].x;
		s_theUVs[2].y = (float)SourceRect.bottom * invSrcHeight;

		s_theUVs[3].x = s_theUVs[1].x;
		s_theUVs[3].y = s_theUVs[2].y;
	}

	if (!mState.TranslationOnly && !clipAxisAlignedQuad(s_theVertices, src ? s_theUVs : 0, mState.ClippingRect))
		return;

	RenderQuad( src, s_theVertices, s_theUVs );
}

//----------------------------------------------------------------------

void UICanvas::Reload(UINarrowString const & /*newTextureName*/)
{
}

