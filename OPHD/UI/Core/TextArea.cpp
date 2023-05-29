#include "TextArea.h"

#include "../../Common.h"
#include "../../Cache.h"

#include <NAS2D/Utility.h>
#include <NAS2D/Renderer/Renderer.h>


void TextArea::font(const std::string& filePath, unsigned int pointSize)
{
	mFont = &fontCache.load(filePath, pointSize);
}


void TextArea::processString()
{
	mFormattedList.clear();

	if (mRect.size.x < 10 || !mFont || text().empty()) { return; }

	const auto tokenList = splitString(text(), ' ');

	std::size_t i = 0;
	int w = 0;
	while (i < tokenList.size())
	{
		std::string line;
		while (w < mRect.size.x && i < tokenList.size())
		{
			int tokenWidth = mFont->width(tokenList[i] + " ");
			w += tokenWidth;
			if (w >= mRect.size.x)
			{
				/**
				 * \todo	In some edge cases where the width of the TextArea is too
				 *			narrow for a single word/token, this will result in an infinite
				 *			loop. This edge case will need to be resolved either by splitting
				 *			the token that's too wide or by simply rendering it as is.
				 */
				//++i;
				break;
			}

			if (tokenList[i] == "\n")
			{
				++i;
				break;
			}
			line += (tokenList[i] + " ");
			++i;
		}
		w = 0;
		mFormattedList.push_back(line);
	}

	mNumLines = static_cast<std::size_t>(mRect.size.y / mFont->height());
}


void TextArea::onResize()
{
	Control::onResize();
	processString();
}


void TextArea::onTextChange()
{
	processString();
}


void TextArea::onFontChange()
{
	processString();
}


void TextArea::update()
{
	draw();
}


void TextArea::draw() const
{
	auto& renderer = NAS2D::Utility<NAS2D::Renderer>::get();

	if (highlight()) { renderer.drawBox(mRect, NAS2D::Color::White); }

	if (!mFont) { return; }

	auto textPosition = mRect.position;
	for (std::size_t i = 0; i < mFormattedList.size() && i < mNumLines; ++i)
	{
		renderer.drawText(*mFont, mFormattedList[i], textPosition, mTextColor);
		textPosition.y += mFont->height();
	}
}
