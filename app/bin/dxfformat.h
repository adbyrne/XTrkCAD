#ifndef HAVE_DXFFORMAT_H
#define HAVE_DXFFORMAT_H

void DxfLayerName(DynString *result, char *name, int layer);
void DxfFormatPosition(DynString *result, int type, double value);
void DxfLineStyle(DynString *result, int isDashed);

void DxfLineCommand(DynString *result, int layer, double x0, double yo, double x1, double y1, int style);
void DxfCircleCommand(DynString *result, int layer, double x, double y, double r, int style);
void DxfArcCommand(DynString *result, int layer, double x, double y, double r, double a0, double a1, int style);
void DxfTextCommand(DynString *result, int layer, double x, double y, double size, char *text);
void DxfUnits(DynString *result);
void DxfDimensionSize(DynString *result, enum DXF_DIMENSIONS dimension);

void DxfPrologue(DynString *result, int layerCount, double x0, double y0, double x1, double y1);
void DxfEpilogue(DynString *result);
#define DXF_INDENT "  "

enum DXF_DIMENSIONS
{
	DXF_DIMTEXTSIZE,
	DXF_DIMARROWSIZE
};
#endif // !HAVE_DXFFORMAT_H

