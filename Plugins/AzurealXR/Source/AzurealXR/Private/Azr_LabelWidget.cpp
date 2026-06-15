

#include "Azr_LabelWidget.h"
#include "Components/TextBlock.h"

void UAzr_LabelWidget::SetLabelText(const FText& InText)
{
	if (LabelTextBlock)
	{
		LabelTextBlock->SetText(InText);
	}
}