#pragma once
#include <string>
#include <vector>

namespace datasFromDCS {
	struct SnapPoint
	{
		long SnapId;
		std::string CompId;
		double t;
		double u;
		double v;
		std::vector<double> Pnt;
		int SnapType;
		int PointType;
	};

	struct ParDirection
	{
		int DirType;
		int LineId;
		std::string CompId;
		SnapPoint Pnt1;
		SnapPoint Pnt2;
		int row;
		int Col;
		int WpDir;
	};

	struct GeomData {
		double L;
		std::vector<double> SP;
		std::vector<double> EP;
		std::vector<double> MP;
		std::vector<double> SV;
		std::vector<double> EV;
	};

	struct RelConstraints
	{
		std::vector<int> Dimensions;
		std::vector<int> Logics;
	};

	struct MessageData
	{
		int Index;
		RelConstraints Constraints;
		bool Reference;
		int Status;
		std::vector<int> SnapStatus;
	};

	struct GeomLineData
	{
		std::string TopoId;
		int GT;
		int Index;
		bool Reference;
		std::vector<double> StartPnt;
		std::vector<double> EndPnt;
	};

	struct GeomArcData
	{
		std::string TopoId;
		int GT;
		int Index;
		bool Reference;
		std::vector<double> StartPnt;
		std::vector<double> MidPnt;
		std::vector<double> EndPnt;
	};

	struct GeomCircleData
	{
		std::string TopoId;
		int GT;
		int Index;
		bool Reference;
		std::vector<double> StartPnt;
		std::vector<double> CenterPnt;
	};

	struct InterPositionNurbsData
	{
		std::string TopoId;
		int GT;
		int Index;
		bool Reference;
		bool IsClosed;
		std::vector<double> Magnitude;
		std::vector<ParDirection> Directions;
		std::vector<double> PickPnts;
	};

	struct NurbsCurveData
	{
		std::string TopoId;
		int GT;
		int Index;
		bool Reference;
		std::vector<double> CPS;
		std::vector<double> W;
		std::vector<double> K;
		double B0;
		double B1;
		int D;
		bool P;
	};

	struct OffsetCurveData
	{
		std::string TopoId;
		int GT;
		int Index;
		bool Reference;
		NurbsCurveData BaseCurve;
		double OffsetDis;
		std::vector<double> OffsetDir;
		int Swp;
		double B0;
		double B1;
	};

	struct EllipseArcData
	{
		std::string TopoId;
		int GT;
		int Index;
		bool Reference;
		std::vector<double> CenterPnt;
		std::vector<double> MinorPnt;
		std::vector<double> MajorPnt;
		double SA;
		double EA;
	};

	struct GeometryPointData
	{
		std::string TopoId;
		int GT;
		std::vector<double> Point;
		int Index;
	};

	struct GeometryData
	{
		GeometryPointData GeometryPointData;
		GeomLineData GeomLineData;
		GeomArcData GeomArcData;
		GeomCircleData GeomCircleData;
		InterPositionNurbsData InterPositionNurbsData;
		NurbsCurveData NurbsCurveData;
		OffsetCurveData OffsetCurveData;
		EllipseArcData EllipseArcData;
	};

	struct WorkPlaneData
	{
		int WPOnSketch;
		int WPOnFace;
		std::string PlaneType;
		std::string WPSketchTopoId;
		int WPSketchId;
		std::string WPFaceTopoId;
		int WPFaceId;
	};

	struct GeometryIdData
	{
		std::string TopoId;
		int GeomId;
	};

	struct VariableData
	{
		std::string VariableName;
		std::string VariableExpression;
		std::string VariableDescription;
		std::string VariableType;
		std::string VariableUnit;
		double VariableValue;
		std::string NewName;
		double SystemValue;
	};

	struct ConstraintsData
	{
		std::string ConstraintType;
		int Type;
		std::string FirstSnapTopoId;
		int FirstSnapId;
		int FirstSnapType;
		std::vector<double> FirstPickPnt;
		std::string SecondSnapTopoId;
		int SecondSnapId;
		int SecondSnapType;
		std::vector<double> SecondPickPnt;
		std::string ThirdSnapTopoId;
		int ThirdSnapId;
		int ThirdSnapType;
		std::vector<double> ThirdPickPnt;
		std::string TextContent;
		int ParamIndex;
		std::vector<double> DimPosition;
		int Direction;
		VariableData DimVariable;
		int DimStatus;
	};

	struct SketchTextsData
	{
		int GT;
		std::vector<GeometryIdData> Geometries;
		std::string TextContent;
		int Index;
	};

	struct SketchParam
	{
		WorkPlaneData WorkPlane;
		std::vector<GeometryData> Geometries;
		std::vector<ConstraintsData> Constraints;
		std::vector<SketchTextsData> SketchTexts;
	};

	struct CurrentFeatureData
	{
		int ID;
		std::string Index;
		std::string Type;
		std::string SubType;
		std::string Name;
		SketchParam Params;
	};

	struct BoxData
	{
		std::vector<double> MinCorner;
		std::vector<double> MaxCorner;
	};

	struct AttributeGeomData
	{
		double Area;
		BoxData Box;
	};

	struct PlaneSurfaceData
	{
		std::vector<double> Norm;
	};

	struct CylSurfaceData
	{
		std::vector<double> Axis;
		std::vector<double> Origin;
		double Radius;
		double StartAngle;
		double EndAngle;
	};

	struct RevSrfAttribute
	{
		std::vector<double> Axis;
		std::vector<double> Origin;
		double StartAngle;
		double EndAngle;
	};

	struct TabCylSrfAttribute
	{
		std::vector<double> Axis;
	};

	struct AttributeSurfaceData
	{
		PlaneSurfaceData PlaneSurfaceData;
		CylSurfaceData CylSurfaceData;
		RevSrfAttribute RevSrfAttribute;
		TabCylSrfAttribute TabCylSrfAttribute;
	};

	struct BaseCurveData
	{
		std::vector<double> CPS;
		std::vector<double> W;
		std::vector<double> K;
		double B0;
		double B1;
		int D;
		bool P;
	};

	struct NurbsHandleData
	{
		double SL;
		double EL;
		std::vector<double> ST;
		std::vector<double> ET;
	};

	struct AttributeLineData
	{
		double A;
	};

	struct AttributeArcData
	{
		double SA;
		double EA;
		double R;
		std::vector<double> CP;
	};

	struct AttributeEllipseData
	{
		double SA;
		double EA;
		double MinorRad;
		double MajorRad;
		std::vector<double> CP;
		std::vector<double> SplitPnts;
		std::vector<double> MinorVec;
		std::vector<double> MajorVec;
	};

	struct AttributeNurbsData
	{
		NurbsHandleData HD;
		std::vector<double> IntpPnts;
		bool IsClosed;
		std::vector<double> CPS;
		std::vector<double> W;
		std::vector<double> K;
		double B0;
		double B1;
		int D;
		bool P;
	};

	struct AttributeCrvOnSrfData
	{
		int RefSrfId;
	};

	struct AttributeOffsetData
	{
		double OffsetDis;
		int Swap;
		double B0;
		double B1;
		std::vector<double> NormalDir;
		std::string BaseCurveType;
		BaseCurveData BaseCurve;
	};

	struct AttributeCurveData
	{
		AttributeLineData AttributeLineData;
		AttributeArcData AttributeArcData;
		AttributeEllipseData AttributeEllipseData;
		AttributeNurbsData AttributeNurbsData;
		AttributeCrvOnSrfData AttributeCrvOnSrfData;
		AttributeOffsetData AttributeOffsetData;
	};

	struct AttributeData
	{
		MessageData Message;
		GeomData Geom;
		AttributeCurveData Curve;
		AttributeSurfaceData Surface;
		AttributeGeomData Geometry;
		std::string Color;
		std::string Opacity;
		double V;
		double A;
		BoxData Box;
		std::string Index;
		std::vector<double> Point;
		std::string Material;
	};

	struct GeometrieData
	{
		std::string GeomTopoId;
		int GeomId;
		int SnapType;
		std::vector<double> PickPnt;
	};

	struct GeometriesData
	{
		int GeomId;
		int SnapType;
		std::vector<double> PickPnt;
		int ArcType;
		std::vector<double> StartPnt;
		std::vector<double> EndPnt;
		std::vector<double> MidPnt;
	};

	struct DimensionDataMessage
	{
		std::string Type;
		double Value;
		std::string Text;
		VariableData DimVariable;
		std::vector<double> Direction;
		std::vector<double> Position;
		std::vector<double> RefPlane;
		std::vector<double> GeomPnts;
		std::vector<double> CenterPnt;
		std::vector<GeometriesData> Geometries;
		int Status;
		double StartAngle;
		double EndAngl;
		std::string ParamName;
		int ParamIndex;
		std::string FeatureIndex;
		int DimStatus;
		int RelView;
	};

	struct ConstraintDataMessage
	{
		int Type;
		std::vector<GeometrieData> Geometries;
		int Status;
		std::vector<double> Position;
		std::vector<double> OffsetVector;
	};

	struct DiscreteInforData
	{
		std::string T;
		std::vector<float> V;
		std::vector<float> TE;
		int FT;
		int FN;
		std::vector<int> I;
		std::vector<float> N;
		int VN;
		int VO;
		std::vector<float> TEArray();
	};

	struct ElementData
	{
		int ID;
		int T;
		int FID;
		bool Visible;
		int G;
		AttributeData A;
		std::vector<GeometrieData> Geometries;
		int Status;
		std::string TextContent;
		std::vector<DiscreteInforData> D;
		ConstraintDataMessage ConstraintData;
		DimensionDataMessage DimensionData;
	};

	struct LoopData
	{
		std::string Type;
		std::vector<int> Coedges;
	};

	struct FaceTopoData
	{
		int ID;
		std::vector<LoopData> Loops;
	};

	struct EdgeTopoData
	{
		int ID;
		std::vector<int> Vertices;
	};

	struct VerticeTopoData
	{
		int ID;
	};

	struct TopoIdData
	{
		int ID;
	};

	struct SketchLoopData
	{
		std::vector<int> Value;
	};

	struct SolidSketchTopoData
	{
		std::vector<FaceTopoData> Faces;
		std::vector<EdgeTopoData> Edges;
		std::vector<VerticeTopoData> Vertices;
		std::vector<TopoIdData> Dimensions;
		std::vector<TopoIdData> Constraints;
		std::vector<TopoIdData> Curves;
		std::vector<TopoIdData> Points;
		std::vector<TopoIdData> SketchTexts;
		std::vector<SketchLoopData> Loops;
		std::vector<TopoIdData> SketchRegion;
		std::vector<TopoIdData> Elements;
	};

	struct DefDataMessage
	{
		std::vector<double> O;
		std::vector<double> N;
		std::vector<double> X;
		double H;
		double W;
		std::vector<double> SP;
		std::vector<double> EP;
	};

	struct EntityData
	{
		int ID;
		int Type;
		int FID;
		bool Visible;
		AttributeData Attr;
		std::string TID;
		std::string Name;
		int OFID;
		SolidSketchTopoData TopoData;
		int VO;
		int VN;
		std::vector<double> SketchLoopDataMatrix;
		bool Status;
		bool Active;
		DefDataMessage DefData;
	};

	struct FeatureData
	{
		int ID;
		std::string Status;
		std::string Type;
		std::string SubType;
		int Flag;
		std::string Name;
		std::string Index;
		std::string Opt;
		int DimensionVisble;
		int Parent;
	};

	struct DeleteVariable
	{
		std::string VariableName;
	};

	struct CompData
	{
		std::string Id;
		std::string Name;
		std::string ProtoId;
		std::vector<double> Mat;
		bool Visible;
		bool Fix;
		std::string Status;
		std::string Index;
		std::string Parent;
		int FeatureId;
		int IsValid;
	};

	struct NewMatingData
	{
		int FID;
		std::string Index;
		int MID;
		std::string Type;
		std::string Name;
		int Align;
		std::string MIns;
		int MEle;
		std::string RIns;
		int REle;
		std::vector<double> MPos;
		std::vector<double> RPos;
		double Offset;
		int Status;
	};

	struct InstanceNode
	{
		std::string Id;
		std::string Name;
		std::vector<InstanceNode> Children;
	};

	struct ViewData
	{
		std::string CategoryName;
		int ID;
		std::string Name;
		std::vector<double> Matrix;
		std::vector<int> RelPMI;
	};

	struct ViewDataParam
	{
		std::vector<ViewData> Value;
	};

	struct ProtoDataMessage
	{
		std::string Id;
		std::string DocId;
		std::string VersionId;
		std::string Name;
		std::vector<CompData> Instances;
		std::vector<NewMatingData> Matings;
		std::vector<FeatureData> FeatureDatas;
		std::vector<InstanceNode> InstanceTree;
		std::vector<ElementData> Elements;
		std::vector<EntityData> Entities;
		std::string Type;
		std::vector<VariableData> Variables;
		std::vector<ViewDataParam> ViewDatas;
	};

	struct IncrementalData {
		std::vector<ElementData> AddElements;
		std::vector<ElementData> ModElements;
		std::vector<int> DelElements;
		std::vector<EntityData> AddEntities;
		std::vector<EntityData> ModEntities;
		std::vector<int> DelEntities;
		std::vector<FeatureData> FeatureDatas;
		std::vector<VariableData> AddVariables;
		std::vector<DeleteVariable> DelVariables;
		std::vector<VariableData> ModVariables;
	};

	struct AssemblyPartData {
		std::vector<CompData> Instances;
		std::vector<NewMatingData> Matings;
		std::vector<FeatureData> FeatureDatas;
		std::vector<InstanceNode> InstanceTree;
		std::vector<ElementData> Elements;
		std::vector<EntityData> Entities;
		std::string Type;
		std::vector<VariableData> Variables;
		std::vector<ProtoDataMessage> ProtoDatas;
		std::vector<ViewDataParam> ViewDatas;
	};

	struct InitDataMessage {
		IncrementalData RedoData;
		std::vector<int> UndoData;
	};

	struct PlaneAxisData {
		int AxisType;
		int LineId;
		std::string CompId;
		SnapPoint Pnt1;
		SnapPoint Pnt2;
	};

	struct InitCmdData
	{
		InitDataMessage InitData;
		std::string ParamData;
		int FeatureId;
	};

	struct ArchData
	{
		std::string Id;
		bool IsPartData;
		std::string Name;
		std::string DocId;
		std::string VersionId;
	};

	struct IncrementalCompData
	{
		std::vector<CompData> AddInstances;
		std::vector<CompData> ModInstances;
		std::vector<std::string> DelInstances;
		std::vector<ArchData> AddProtos;
		std::vector<ArchData> ModProtos;
		std::vector<std::string> DelProtos;
		std::vector<NewMatingData> AddMatings;
		std::vector<NewMatingData> ModMatings;
		std::vector<NewMatingData> DelMatings;
		std::vector<ElementData> AddElements;
		std::vector<ElementData> ModElements;
		std::vector<int> DelElements;
		std::vector<EntityData> AddEntities;
		std::vector<EntityData> ModEntities;
		std::vector<int> DelEntities;
		std::vector<FeatureData> FeatureDatas;
		std::vector<ProtoDataMessage> ProtoDatas;
	};

	struct DiscreteData {
		std::string T;
		std::vector<float> V;
		std::vector<int> I;
		std::vector<float> N;
		std::vector<float> TE;
	};

	struct MatrixData {
		std::vector<double> mat;
	};

	struct PreviewDataMessage
	{
		int T;
		std::vector<DiscreteData> D;
		std::vector<MatrixData> Matri;
	};

	struct TransformInstanceData
	{
		std::string key;
		std::vector<double> value;
	};

	struct TransformDataMessage
	{
		std::vector<TransformInstanceData> TransformInstance;
	};

	struct MatInforData
	{
		std::vector<double> mat;
	};

	struct AnimationInstanceData
	{
		std::string key;
		std::vector<MatInforData> value;
	};

	struct AnimationDataMessage
	{
		std::vector<AnimationInstanceData> AnimationInstance;
	};

	struct DistanceHandleData
	{
		std::vector<double> Origin;
		std::vector<double> Direction;
		std::vector<double> Direction2;
		double Length;
	};

	struct AngleHandleData
	{
		std::vector<double> Origin;
		std::vector<double> Direction1;
		std::vector<double> Direction2;
		double Radius;
		double Angle;
	};

	struct RadiusHandleData
	{
		std::vector<double> Origin;
		std::vector<double> Direction;
		double Length;
	};

	struct TangentHandleData
	{
		double StartLength;
		double EndLength;
		std::vector<double> StartTang;
		std::vector<double> EndTang;
		bool StartRotate;
		bool EndRotate;
	};

	struct PointHandleData
	{
		std::vector<SnapPoint> Points;
		std::vector<int> Status;
	};

	struct MatrixHandleData
	{
		std::vector<double> Origin;
		std::vector<double> VectorX;
		std::vector<double> VectorY;
		std::vector<double> VectorZ;
	};

	struct DefineDataMessage
	{
		DistanceHandleData DistanceHandleData;
		AngleHandleData AngleHandleData;
		RadiusHandleData RadiusHandleData;
		TangentHandleData TangentHandleData;
		PointHandleData PointHandleData;
		MatrixHandleData MatrixHandleData;
	};

	struct HandleDataMessage
	{
		std::string RefParam;
		int Type;
		DefineDataMessage DefineData;
		DimensionDataMessage DimensionData;
	};

	struct TopoDataMessage
	{
		int Id;
		std::string TID;
		std::string InstanceId;
		std::string InstanceIndex;
	};

	struct SerializePreviewData
	{
		std::vector<PreviewDataMessage> PreviewData;
		std::vector<TransformDataMessage> TransformData;
		std::vector<AnimationDataMessage> AnimationData;
		std::vector<HandleDataMessage> HandleData;
		std::string ParamData;
		std::vector<TopoDataMessage> TopoData;
		int SweepDirectionData;
	};

	struct SolidPreviewData
	{
		int Type;
		std::vector<DiscreteData> D;
	};

	struct SpecialPreviewInforData
	{
		std::vector<PreviewDataMessage> MiddleData;
		std::vector<int> OldSrfData;
		std::vector<int> NewSrfData;
		std::vector<SolidPreviewData> PreviewData;
		std::vector<TopoDataMessage> TopoData;
		std::vector<HandleDataMessage> HandleData;
		std::string ParamData;
	};

	struct SketchPreviewData
	{
		std::vector<ElementData> VariableDataPreviewData;
		std::vector<TopoDataMessage> TopoData;
	};

	struct GapsData
	{
		double GapDis;
		int ID1;
		int SnapType1;
		std::vector<double> Point1;
		int ID2;
		int SnapType2;
		std::vector<double> Point2;
	};

	struct OverLapsData
	{
		std::vector<int> OverLap;
	};

	struct SketchCheckData
	{
		std::vector<GapsData> Gaps;
		std::vector<OverLapsData> OverLaps;
		std::vector<int> ShortCurves;
		int Status;
	};

	struct DatumPreviewData
	{
		DefDataMessage DefData;
		std::vector<HandleDataMessage> HandleData;
		std::vector<TopoDataMessage> TopoData;
	};

	struct InitOffsetCurveData
	{
		bool OffsetOnPlane;
		int OffsetType;
		VariableData DimVariable;
		std::vector<double> OffsetPnt;
		int OffsetTrans;
		int OffsetMoveCopy;
	};

	struct WorkPlaneMatrixData
	{
		std::vector<double> Matrix;
	};

	struct MeasureInforData
	{
		std::string Value;
	};

	struct VariableDataMessage
	{
		std::vector<VariableData> Value;
	};

	struct DimensionDataInforMessage
	{
		std::vector<ElementData> Dimensions;
	};

	struct ErrorStringData
	{
		std::vector<HandleDataMessage> HandleData;
		std::string ParamData;
	};

	struct MassPropertyData
	{
		std::vector<double> BaryCenter;
		double Area;
		double Volume;
	};

	struct IntegerValue
	{
		int Value;
	};

	struct SaveFileData
	{
		std::string Value;
	};

	struct SaveData
	{
		std::vector<SaveFileData> SaveFiles;
	};

	struct IdData
	{
		std::string Value;
	};

	struct DataMessage {
		IncrementalData IncrementalData;
		AssemblyPartData AssemblyPartData;
		InitCmdData InitCmdData;
		IncrementalCompData IncrementalCompData;
		SerializePreviewData SerializePreviewData;
		SpecialPreviewInforData SpecialPreviewInforData;
		SketchPreviewData SketchPreviewData;
		SketchCheckData SketchCheckData;
		DatumPreviewData DatumPreviewData;
		InitOffsetCurveData InitOffsetCurveData;
		WorkPlaneMatrixData WorkPlaneMatrixData;
		MeasureInforData MeasureInforData;
		VariableDataMessage VariableDataMessage;
		DimensionDataInforMessage DimensionDataInforMessage;
		ElementData ElementData;
		ErrorStringData ErrorStringData;
		MassPropertyData MassPropertyData;
		IntegerValue IntegerValue;
		SaveData SaveData;
		IdData IdData;
	};

	struct ErrorMessage
	{
		std::string Code;
		std::string Description;
	};

	struct FlatBufferDoc {
		DataMessage Data;
		int EntityType;
		std::string Mode;
		int Status;
		ErrorMessage Error;
		int RollFeatureId;
		std::vector<CurrentFeatureData> CurrentFeature;
		bool SketchWorkState;
		long UndoStepNum;
		long RedoStepNum;
		double ModelTime;
		double ResultTime;
	};
}