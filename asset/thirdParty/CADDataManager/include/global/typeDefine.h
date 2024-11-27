#pragma once
enum class DocumentType {
    DOCUMENT_PART = 1,
    DOCUMENT_ASSEMBLT = 2
};

enum class EntityType {
    ENTITY_UNKNOWN = 0,
    ENTITY_PART = 1,
    ENTITY_ASSEMBLY = 2,
    ENTITY_ENTIYY = 3,
    ENTITY_SOLID = 4,
    ENTITY_SHELL = 5,
    ENTITY_SKETCH = 6,
    ENTITY_SKETCHREGION = 7,
    ENTITY_CURVE = 8,
    ENTITY_WORKPLANE = 9,
    ENTITY_DATUMPLANE = 10,
    ENTITY_DATUMLINE = 11,
    ENTITY_DATUMPOINT = 12,
    ENTITY_PARTFILE = 13,
    ENTITY_ASSEMBLYFILE = 14,
    ENTITY_UNDOREDO = 15,
    ENTITY_DELETE = 16,
    ENTITY_VISIBLE = 17,
    ENTITY_VIEW = 18,
    ENTITY_FACE = 19,
    ENTITY_EDGE = 20,
    ENTITY_VERTEX = 21,
    ENTITY_POINT = 22,
    ENTITY_MARKER = 23,
    ENTITY_SURFACE = 24,
    ENTITY_CONSTRAINT = 25,
    ENTITY_DIMENSION = 26,
    ENTITY__3DDIMENSION = 27,
    ENTITY_VARIABLE = 28,
    ENTITY_DRIVEDIMENSION = 29,
    ENTITY_LABEL = 30,
    ENTITY_GROUP = 31,
    ENTITY_FILE = 32,
    ENTITY_PROPERTY = 33,
    ENTITY_DDIMENSION = 34,
    ENTITY_MESH = 36,
    ENTITY_SNAPPOINT = 97,
    ENTITY_POINTONCURVE = 98,
    ENTITY_ORIGINPOINT = 99,
    ENTITY_TYPE_ERROE = 100
};

enum class ElementType {
    ELEMENT_UNKNOWN = 0,
    ELEMENT_SKETCHREGION = 7,
    ELEMENT_CURVE = 8,
    ELEMENT_WORKPLANE = 9,
    ELEMENT_DATUMPLANE = 10,
    ELEMENT_DATUMLINE = 11,
    ELEMENT_DATUMPOINT = 12,
    ELEMENT_FACE = 19,
    ELEMENT_EDGE = 20,
    ELEMENT_VERTEX = 21,
    ELEMENT_POINT = 22,
    ELEMENT_MARKER = 23,
    ELEMENT_SURFACE = 24,
    ELEMENT_CONSTRAINT = 25,
    ELEMENT_DIMENSION = 26,
    ELEMENT_3DDIMENSION = 27,
    ELEMENT_VARIABLE = 28,
    ELEMENT_DRIVEDIMENSION = 29,
    ELEMENT_LABEL = 30,
    ELEMENT_GROUP = 31,
    ELEMENT_FILE = 32,
    ELEMENT_PROPERTY = 33,
    ELEMENT_DDIMENSION = 34, //DrivingDimension
    ELEMENT_SKETCHTEXT = 35,
    ELEMENT_MESH = 36,
    ELEMENT_SNAPPOINT = 97,
    ELEMENT_POINTONCURVE = 98,
    ELEMENT_ORIGINPOINT = 99,
    ELEMENT_ERROE = 100
};

enum class GeomType {
    GEOM_GEOM_UNKNOWN = 0,
    GEOM_POINT = 1, //point, //
    // 线几何类型
    GEOM_LINE = 10, // line, //
    GEOM_CIRCLE = 11, // circle, //
    GEOM_ARC = 12, // arc, //
    GEOM_ELLIPSE = 13, // ellipse, //
    GEOM_NURBSCURVE = 14, // nurbs, //
    GEOM_INTERPOLATIONNURBSCURVE = 15, // Interpolation_Nurbs, //
    GEOM_CONTROLNURBSCURVE = 16, // Control_Nurbs, //
    GEOM_CURVEONSURFACE = 17, // crvOnSrf, //
    GEOM_OFFSCURVE = 18, // offsetCurve, //
    GEOM_ELLIPSEARC = 19, // ellipseArc, //

    //面
    GEOM_PLANE = 50, // "plane"
    GEOM_CYLINDER = 51, // cylinder, //
    GEOM_RULE = 52, // rule, //
    GEOM_COONS = 53, // coons, //
    GEOM_REVOLUTION = 54, // revolution, //
    GEOM_TABCYLINDER = 55, // tabCylinder, //
    GEOM_NURBSSURFACE = 56, // nurbs, //
    GEOM_BLEND = 57, // blend, //
    GEOM_DEFORM = 58, // deform, //
    GEOM_MONODRIVEN = 59, // monoDriven, //
    GEOM_BIDRIVEN = 60, // biDriven, //
    GEOM_DAVID = 61, // david, //
    GEOM_OFFSSRF = 62, // offs, //
    GEOM_CONE = 63, // cone, //
    GEOM_SPHERE = 64, // sphere, //
    GEOM_TORUS = 65, // torus, //
    GEOM_ERROR = 100// ERROR, //
};

enum class DirectionType {
    Direction_TWO_POINTS = 1,
    Direction_LINE = 2,
    Direction_PT_VERTEXS = 19,
};

/* 视图立方体 */
enum class ViewCubeMeshName {
    /* 面 */
    FACE_FRONT = 1,
    FACE_LEFT = 2,
    FACE_BACK = 3,
    FACE_RIGHT = 4,
    FACE_TOP = 5,
    FACE_BOTTOM = 6,

    /* 字（与面效果相同） */
    TEXT_FRONT = 7,
    TEXT_LEFT = 8,
    TEXT_BACK = 9,
    TEXT_RIGHT = 10,
    TEXT_TOP = 11,
    TEXT_BOTTOM = 12,

    /* 角（第一个TOP/BOTTOM表示上下，第二个TOP/BOTTOM表示前后） */
    TOP_CORNER_LEFT_BOTTOM = 13,
    TOP_CORNER_RIGHT_BOTTOM = 14,
    BOTTOM_CORNER_LEFT_TOP = 15,
    BOTTOM_CORNER_RIGHT_TOP = 16,
    TOP_CORNER_RIGHT_TOP = 17,
    TOP_CORNER_LEFT_TOP = 18,
    BOTTOM_CORNER_RIGHT_BOTTOM = 19,
    BOTTOM_CORNER_LEFT_BOTTOM = 20,

    /* 边（第一个TOP/BOTTOM表示上下，第二个TOP/BOTTOM表示前后） */
    RIGHT_EDGE_RIGHT = 21,
    RIGHT_EDGE_LEFT = 22,
    TOP_EDGE_BOTTOM = 23,
    TOP_EDGE_RIGHT = 24,
    TOP_EDGE_TOP = 25,
    TOP_EDGE_LEFT = 26,
    BOTTOM_EDGE_TOP = 27,
    BOTTOM_EDGE_BOTTOM = 28,
    BOTTOM_EDGE_RIGHT = 29,
    BOTTOM_EDGE_LEFT = 30,
    LEFT_EDGE_LEFT = 31,
    LEFT_EDGE_RIGHT = 32,

    /* 控制箭头 */
    UPTRI = 33,
    DOWNTRI = 34,
    LEFTTRI = 35,
    RIGHTTRI = 36,
    // 逆时针
    LEFTUP = 37,
    // 顺时针
    RIGHTUP = 38

};
