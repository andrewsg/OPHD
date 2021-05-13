#pragma once

#include "Core/Control.h"


#include <vector>
#include <NAS2D/EventHandler.h>
#include <NAS2D/Resource/Image.h>
#include <NAS2D/Signal/Signal.h>


class NotificationArea : public Control
{
public:
	enum class NotificationType
	{
		Critical,
		Information,
		Warning
	};


	struct Notification
	{
		std::string message{ "" };
		NotificationType type{ NotificationType::Information };
	};

	const int Width = 48;
	
	using NotificationCallback = NAS2D::Signal<int>;

public:
	NotificationArea();
	~NotificationArea() override;

	void push(const std::string& message, NotificationType type);

	void clear()
	{
		mNotificationList.clear();
		mNotificationRectList.clear();
	}

	void update() override;

	NotificationCallback& notificationClicked() { return mNotificationClicked; }

protected:
	void onMouseDown(NAS2D::EventHandler::MouseButton, int, int);

	void positionChanged(int, int);
	void onResize() override;

private:
	void updateRectListPositions();

	const NAS2D::Image& mIcons;
	std::vector<Notification> mNotificationList;
	std::vector<NAS2D::Rectangle<int>> mNotificationRectList;

	NotificationCallback mNotificationClicked;
};
