// ======================================================================
//
// SwgCuiTcgControl.h
// copyright (c) 2008 Sony Online Entertainment LLC
//
// ======================================================================

#ifndef INCLUDED_SwgCuiTcgControl_H
#define INCLUDED_SwgCuiTcgControl_H

#include "clientUserInterface/CuiLayer.h"
#include "UIWidget.h"

#include <cstddef>
#include <vector>

// ----------------------------------------------------------------------

namespace libEverQuestTCG
{
	class Window;
};

class Texture;
class UIImage;

// ======================================================================

class SwgCuiTcgControl : public UIWidget
{
public:
	SwgCuiTcgControl();
	virtual ~SwgCuiTcgControl();

	virtual void Render(UICanvas & canvas) const;
	virtual bool ProcessMessage(const UIMessage & msg);

	virtual UIBaseObject * Clone() const;
	virtual UIStyle * GetStyle() const;

	void setEqTcgWindow(libEverQuestTCG::Window * eqTcgWindow);
	libEverQuestTCG::Window * getEqTcgWindow() const;
	void initializeEqTcgWindow();

	void setImage(UIImage * image);
	UIImage * getImage() const;

	void alter(float deltaTime);
	bool dispatchIntegrationTestClick(unsigned normalizedX, unsigned normalizedWidth, unsigned normalizedY, unsigned normalizedHeight);

	virtual void OnSizeChanged(UISize const & newSize, UISize const & oldSize);
	virtual void OnLocationChanged(UIPoint const & newLocation, UIPoint const & oldLocation);
	virtual bool IsA(UITypeID const Type) const;
	virtual void SetSelected(const bool selected);

protected:

private: //disabled
	void fetchTexture();
	bool prepareHorizontalSampleMap(std::size_t sourceWidth, std::size_t destinationWidth) const;

	SwgCuiTcgControl(SwgCuiTcgControl const & rhs);
	SwgCuiTcgControl& operator= (SwgCuiTcgControl const & rhs);

private:

	libEverQuestTCG::Window * m_eqTcgWindow;
	UIImage * m_image;
	Texture * m_texture;
	mutable bool m_reportedFirstFrame;
	bool m_reportedInputDispatch;
	bool m_reportedInputMapping;
	mutable std::vector<std::size_t> m_horizontalSampleOffsets;
	mutable std::size_t m_horizontalMapSourceWidth;
	mutable std::size_t m_horizontalMapDestinationWidth;
};

// ----------------------------------------------------------------------

inline libEverQuestTCG::Window * SwgCuiTcgControl::getEqTcgWindow() const
{
	return m_eqTcgWindow;
}

// ----------------------------------------------------------------------

inline UIImage * SwgCuiTcgControl::getImage() const
{
	return m_image;
}


// ----------------------------------------------------------------------

inline bool SwgCuiTcgControl::IsA(const UITypeID Type) const
{
	return Type == TUIWebBrowser || UIWidget::IsA(Type);
}

// ======================================================================

#endif
