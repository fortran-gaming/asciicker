
#include "game.h"
#include <string.h>

extern World* world;
const ItemProto* item_proto_lib;

Item* CreateItem()
{
	Item* item = (Item*)malloc(sizeof(Item));
	memset(item, 0, sizeof(Item));
	return item;
}

void DestroyItem(Item* item)
{
	if (item->inst)
		DeleteInst(item->inst);
	free(item);
}

void Inventory::FocusNext(int dx, int dy)
{
	if (focus<0 || my_items<=1 || !dx && !dy || dx && dy)
		return;

	//int ccw_clip[2] = { dx + dy, dy - dx };
	//int cw_clip[2] = { dx - dy, dy + dx };

	int i = focus;
	Item* item = my_item[i].item;
	int x0 = my_item[i].xy[0];
	int x1 = x0 + (item->proto->sprite_2d->atlas->width + 1) / 4;
	int y0 = my_item[i].xy[1];
	int y1 = y0 + (item->proto->sprite_2d->atlas->height + 1) / 4;

	int fx0 = 2*x0;
	int fx1 = 2*x1;
	int fy0 = 2*y0;
	int fy1 = 2*y1;

	bool major_x = dx*dx>dy*dy;

	int fx,fy;
	if (major_x)
	{
		fx = 2*(dx>0 ? x1 : x0);
		fy = y0+y1;
	}
	else
	{
		fx = x0+x1;
		fy = 2*(dy>0 ? y1 : y0);
	}

	unsigned int best_e = 0xffff;
	int best_i = -1;

	for (int i = 0; i < my_items; i++)
	{
		if (i == focus)
			continue;

		Item* item = my_item[i].item;
		int x0 = my_item[i].xy[0];
		int x1 = x0 + (item->proto->sprite_2d->atlas->width + 1) / 4;
		int y0 = my_item[i].xy[1];
		int y1 = y0 + (item->proto->sprite_2d->atlas->height + 1) / 4;

		int cx,cy;
		int px,py;
		if (major_x)
		{
			if (fy < 2*y0)
				py = 2*y0;
			else
			if (fy > 2*y1)
				py = 2*y1;
			else
				py = fy;

			cx = px = 2*(dx>0 ? x0 : x1);
			cy = y0+y1;	

			if (cy<fy0)
				cy = fy0;
			if (cy>fy1)
				cy = fy1;
		}
		else
		{
			if (fx < 2*x0)
				px = 2*x0;
			else
			if (fx > 2*x1)
				px = 2*x1;
			else
				px = fx;
			
			cx = x0+x1;
			cy = py = 2*(dy>0 ? y0 : y1);

			if (cx<fx0)
				cx = fx0;
			if (cx>fx1)
				cx = fx1;
		}

		// from now coords are doubled, find_pos is already doubled!
		int vx = px - fx;
		int vy = py - fy;

		cx -= fx;
		cy -= fy;

		// int dot = cx * dx + cy * dy;
		// if (dot > 0)

		//int cw = vx * cw_clip[0] + vy * cw_clip[1];
		//int ccw = vx * ccw_clip[0] + vy * ccw_clip[1];

		// +/- 45 angle trimming
		//if (cw >=0 && ccw >= 0)

		if (vx * dx + vy * dy >=0)
		{
			int e = major_x ? vx * vx + 4 * vy * vy + cy * cy: 4 * vx * vx + vy * vy + cx * cx;

			if (e < best_e)
			{
				best_i = i;
				best_e = e;
			}
		}
	}

	if (best_i<0)
		return;

	animate_scroll = true;
	smooth_scroll = scroll;

	SetFocus(best_i);
}

void Inventory::SetFocus(int index)
{
	focus = index;
}

bool Inventory::InsertItem(Item* item, int xy[2])
{
	assert(item && item->inst && item->purpose == Item::WORLD);
	if (my_items < max_items)
	{
		DeleteInst(item->inst);

		item->inst = 0;
		item->purpose = Item::OWNED;

		my_item[my_items].xy[0] = xy[0];
		my_item[my_items].xy[1] = xy[1];
		my_item[my_items].item = item;
		my_item[my_items].in_use = false;


		// set bitmask
		int x0 = my_item[my_items].xy[0];
		int x1 = x0 + (item->proto->sprite_2d->atlas->width + 1) / 4;
		int y0 = my_item[my_items].xy[1];
		int y1 = y0 + (item->proto->sprite_2d->atlas->height + 1) / 4;
		for (int y = y0; y < y1; y++)
		{
			for (int x = x0; x < x1; x++)
			{
				int i = x + y * width;
				bitmask[i >> 3] |= 1 << (i & 7);
			}
		}

		my_items++;
		focus = my_items - 1;
	}

	animate_scroll = true;
	smooth_scroll = scroll;


	return true;
}

bool Inventory::RemoveItem(int index, float pos[3], float yaw)
{
	assert(index >= 0 && index < my_items && my_item[index].item &&
		my_item[index].item->purpose == Item::OWNED && !my_item[index].item->inst);

	int flags = INST_USE_TREE | INST_VISIBLE;

	Item* item = my_item[index].item;

	// clear bitmask
	int x0 = my_item[index].xy[0];
	int x1 = x0 + (item->proto->sprite_2d->atlas->width + 1) / 4;
	int y0 = my_item[index].xy[1];
	int y1 = y0 + (item->proto->sprite_2d->atlas->height + 1) / 4;
	for (int y = y0; y < y1; y++)
	{
		for (int x = x0; x < x1; x++)
		{
			int i = x + y * width;
			bitmask[i >> 3] &= ~(1 << (i&7));
		}
	}

	if (pos)
	{
		item->purpose = Item::WORLD;
		item->inst = CreateInst(world, item, flags, pos, yaw);
		assert(item->inst);
	}
	else
	{
		DestroyItem(item);
	}

	my_items--; // fill gap, preserve order!

	for (int i = index; i < my_items; i++)
		my_item[i] = my_item[i + 1];
	my_item[my_items].item = 0;

	// find closest item in y, if multiple in x?
	if (focus > index || focus == my_items)
		SetFocus(focus - 1);
	else
		SetFocus(focus);	

	animate_scroll = true;
	smooth_scroll = scroll;

	return true;
}