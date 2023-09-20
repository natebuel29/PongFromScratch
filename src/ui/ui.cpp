#include "ui.h"

void update_ui(UIState *ui)
{
    ui->labelCount = 0;
}

void do_text(UIState *ui, Vec2 pos, char *text)
{
    if (ui->labelCount < MAX_LABELS)
    {
        Label *l = &ui->labels[ui->labelCount++];

        l->text = text;
        l->pos = pos;
    }
    else
    {
        NB_ASSERT(0, "REACHED MAX AMOUNT OF LABELS");
    }
}