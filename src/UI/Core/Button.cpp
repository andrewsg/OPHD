#include "Button.h"

#include "../../Common.h"

using namespace std;

Button::Button():	mState(STATE_NORMAL),
					mType(BUTTON_NORMAL),
					mUsesImage(false),
					mMouseHover(false)
{
	Utility<EventHandler>::get().mouseButtonDown().connect(this, &Button::onMouseDown);
	Utility<EventHandler>::get().mouseButtonUp().connect(this, &Button::onMouseUp);
	Utility<EventHandler>::get().mouseMotion().connect(this, &Button::onMouseMotion);
	hasFocus(true);

	mSkinNormal.push_back(Image("ui/skin/button_top_left.png"));
	mSkinNormal.push_back(Image("ui/skin/button_top_middle.png"));
	mSkinNormal.push_back(Image("ui/skin/button_top_right.png"));
	mSkinNormal.push_back(Image("ui/skin/button_middle_left.png"));
	mSkinNormal.push_back(Image("ui/skin/button_middle_middle.png"));
	mSkinNormal.push_back(Image("ui/skin/button_middle_right.png"));
	mSkinNormal.push_back(Image("ui/skin/button_bottom_left.png"));
	mSkinNormal.push_back(Image("ui/skin/button_bottom_middle.png"));
	mSkinNormal.push_back(Image("ui/skin/button_bottom_right.png"));

	mSkinHover.push_back(Image("ui/skin/button_hover_top_left.png"));
	mSkinHover.push_back(Image("ui/skin/button_hover_top_middle.png"));
	mSkinHover.push_back(Image("ui/skin/button_hover_top_right.png"));
	mSkinHover.push_back(Image("ui/skin/button_hover_middle_left.png"));
	mSkinHover.push_back(Image("ui/skin/button_hover_middle_middle.png"));
	mSkinHover.push_back(Image("ui/skin/button_hover_middle_right.png"));
	mSkinHover.push_back(Image("ui/skin/button_hover_bottom_left.png"));
	mSkinHover.push_back(Image("ui/skin/button_hover_bottom_middle.png"));
	mSkinHover.push_back(Image("ui/skin/button_hover_bottom_right.png"));

	mSkinPressed.push_back(Image("ui/skin/button_pressed_top_left.png"));
	mSkinPressed.push_back(Image("ui/skin/button_pressed_top_middle.png"));
	mSkinPressed.push_back(Image("ui/skin/button_pressed_top_right.png"));
	mSkinPressed.push_back(Image("ui/skin/button_pressed_middle_left.png"));
	mSkinPressed.push_back(Image("ui/skin/button_pressed_middle_middle.png"));
	mSkinPressed.push_back(Image("ui/skin/button_pressed_middle_right.png"));
	mSkinPressed.push_back(Image("ui/skin/button_pressed_bottom_left.png"));
	mSkinPressed.push_back(Image("ui/skin/button_pressed_bottom_middle.png"));
	mSkinPressed.push_back(Image("ui/skin/button_pressed_bottom_right.png"));

}


Button::~Button()
{
	Utility<EventHandler>::get().mouseButtonDown().disconnect(this, &Button::onMouseDown);
	Utility<EventHandler>::get().mouseButtonUp().disconnect(this, &Button::onMouseUp);
	Utility<EventHandler>::get().mouseMotion().disconnect(this, &Button::onMouseMotion);
}


void Button::type(Type type)
{
	type ? mType = BUTTON_TOGGLE : mType = BUTTON_NORMAL;
}


void Button::toggle(bool toggle)
{
	toggle ? mState = STATE_PRESSED : mState = STATE_NORMAL;
}


bool Button::toggled() const
{
	if(mState == STATE_PRESSED)
		return true;

	return false;
}


void Button::image(const std::string& path)
{
	mImage = Image(path);
	mUsesImage = true;
}


bool Button::hasImage() const
{
	return mImage.loaded();
}


void Button::onMouseDown(EventHandler::MouseButton button, int x, int y)
{
	if(!enabled() || !visible() || !hasFocus())
		return;

	if(button == EventHandler::BUTTON_LEFT)
	{
		Point_2d click(x, y);


		if(isPointInRect(click, rect()))
		{
			if(mType == BUTTON_NORMAL)
			{
				mState = STATE_PRESSED;
			}
			else
			{
				mState == STATE_PRESSED ? mState = STATE_NORMAL : mState = STATE_PRESSED;
				mCallback();
			}
		}
	}
}


void Button::onMouseUp(EventHandler::MouseButton button, int x, int y)
{
	if(!enabled() || !visible() || !hasFocus())
		return;

	if(button == EventHandler::BUTTON_LEFT)
	{
		Point_2d click(x, y);
		
		if(mType == BUTTON_NORMAL)
		{
			mState = STATE_NORMAL;

			if(isPointInRect(click, rect()))
				mCallback();
		}
	}
}


void Button::onMouseMotion(int x, int y, int dX, int dY)
{
	if (pointInRect_f(x, y, rect()))
	{
		mMouseHover = true;
		return;
	}

	mMouseHover = false;
}


void Button::update()
{
	draw();
}

/**
 * Draws the button.
 */
void Button::draw()
{
	if(!visible())
		return;

	Renderer& r = Utility<Renderer>::get();

	if (mState == STATE_NORMAL)
	{
		//r.drawBoxFilled(rect(), 225, 225, 225);
		//r.drawBox(rect(), 175, 175, 175);

		r.drawImageRect(rect().x(), rect().y(), rect().width(), rect().height(), mSkinNormal);
	}
	else //(mState == STATE_PRESSED)
	{
		if (mType == BUTTON_NORMAL)
			//r.drawBoxFilled(rect(), 200, 215, 245);
			r.drawImageRect(rect().x(), rect().y(), rect().width(), rect().height(), mSkinNormal);
		else //(mType == BUTTON_TOGGLE)
			//r.drawBoxFilled(rect(), 170, 210, 245);
			r.drawImageRect(rect().x(), rect().y(), rect().width(), rect().height(), mSkinPressed);

		//r.drawBox(rect(), 0, 85, 155);
	}

	if (enabled() && mMouseHover && mState != STATE_PRESSED)
		r.drawImageRect(rect().x(), rect().y(), rect().width(), rect().height(), mSkinHover);
		//r.drawBox(rect(), 0, 120, 215);

	if (mUsesImage)
		r.drawImage(mImage, rect().x() + (rect().width() / 2) - (mImage.width() / 2), rect().y() + (rect().height() / 2) - (mImage.height() / 2));
	else
		if (fontSet() && !text().empty())
			r.drawText(font(), text(), static_cast<int>(rect().x() + (rect().width() / 2) - (font().width(text()) / 2)), static_cast<int>(rect().y() + (rect().height() / 2) - (font().height() / 2)), 255, 255, 255);

	if (!enabled())
		r.drawBoxFilled(rect(), 125, 125, 125, 100);
}


