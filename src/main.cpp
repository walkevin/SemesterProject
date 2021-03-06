#include <igl/avg_edge_length.h>
#include <igl/barycenter.h>
#include <igl/comb_cross_field.h>
#include <igl/comb_frame_field.h>
#include <igl/comiso/miq.h>
#include <igl/compute_frame_field_bisectors.h>
#include <igl/cross_field_missmatch.h>
#include <igl/cut_mesh_from_singularities.h>
#include <igl/find_cross_field_singularities.h>
#include <igl/local_basis.h>
#include <igl/readOFF.h>
#include <igl/rotate_vectors.h>
#include <igl/comiso/nrosy.h>
#include <igl/viewer/Viewer.h>
#include <sstream>

#include <igl/serialize.h>
#include <igl/xml/serialize_xml.h>

#include <igl/cut_mesh.h>
#include <igl/cotmatrix.h>
//DEBUG
#include <igl/jet.h>
// Input mesh
Eigen::MatrixXd V;
Eigen::MatrixXi F;

// Face barycenters
Eigen::MatrixXd B;

// Scale for visualizing the fields
double global_scale;
bool extend_arrows = false;

// Cross field
Eigen::MatrixXd X1,X2;

// Bisector field
Eigen::MatrixXd BIS1, BIS2;

// Combed bisector
Eigen::MatrixXd BIS1_combed, BIS2_combed;

// Per-corner, integer mismatches
Eigen::MatrixXi MMatch;

// Field singularities
Eigen::VectorXi isSingularity, singularityIndex;

// Per corner seams
Eigen::MatrixXi Seams;

// Combed field
Eigen::MatrixXd X1_combed, X2_combed;


// Global parametrization (with seams)
Eigen::MatrixXd UV_seams;
Eigen::MatrixXi FUV_seams;

// Global parametrization
Eigen::MatrixXd UV;
Eigen::MatrixXi FUV;

// Serialization state
struct MIQState : public igl::Serializable
{
  Eigen::MatrixXd UV;
  Eigen::MatrixXi FUV;

  // You have to define this function to
  // register the fields you want to serialize
  virtual void InitSerialization()
  {
    this->Add(UV  , "UV");
    this->Add(FUV , "FUV");
  }

  virtual ~MIQState(){}

  void printDiff(const MIQState &other, std::ostream& out = std::cout){
    std::cout << "Start Diff..\n";
    out << "Checking UV..\n";
    std::vector<double> errs;
    int nErrorsUV = 0;
    for(int i = 0; i < UV.rows(); i++){
      double diff = (UV.row(i) - other.UV.row(i)).norm();
      errs.push_back(diff);
      if(diff > 1e-06){
        if(nErrorsUV == 0)
          out << "Index\tthisUV\totherUV\n";
        out << i << "\t" << UV(i,0) << "\t" << UV(i,1) << "\t" << other.UV(i,0) << "\t" << other.UV(i,1) << std::endl;
        nErrorsUV++;
      }
    }
    std::cout << "Average error: " << std::accumulate(errs.begin(), errs.end(), 0.) / static_cast<double>(errs.size());
    std::cout << " +/- " << std::accumulate(errs.begin(), errs.end(), 0., [](double init, double value){return init + value * value;} ) / static_cast<double>(errs.size() -1) << std::endl;
    std::cout << "Max absolute error: " << *std::max_element(errs.begin(), errs.end()) << std::endl;

    out << "Checking FUV..\n";
    int nErrorsFUV = 0;
    for(int i = 0; i < FUV.rows(); i++){
      if(FUV.row(i) != other.FUV.row(i)){
        if(nErrorsFUV == 0)
          out << "Index\tthisFUV\totherFUV\n";
        out << i << "\t" << FUV(i,0) << "\t" << FUV(i,1) << "\t" << other.FUV(i,0) << "\t" << other.FUV(i,1) << std::endl;
        nErrorsFUV++;
      }
    }

    std::cout << "Finished.\n" << nErrorsUV << " errors in UV.\n" << nErrorsFUV << " errors in FUV.\n";
  }
};

// Create a texture that hides the integer translation in the parametrization
void line_texture(Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic> &texture_R,
                  Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic> &texture_G,
                  Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic> &texture_B)
  {
    unsigned size = 128;
    unsigned lineWidth = 3;
    texture_R.setConstant(size, size, 255);
    for (unsigned i=0; i<size; ++i){
      for (unsigned j=0; j<lineWidth; ++j)
        texture_R(i,j) = 0;
      for (unsigned j=size-lineWidth; j<size; ++j)
        texture_R(i,j) = 0;
    }

    for (unsigned j=0; j<size; ++j){
      for (unsigned i=0; i<lineWidth; ++i)
        texture_R(i,j) = 0;
      for (unsigned i=size-lineWidth; i<size; ++i)
        texture_R(i,j) = 0;
    }

    texture_G = texture_R;
    texture_B = texture_R;
  }

bool key_down(igl::viewer::Viewer& viewer, unsigned char key, int modifier)
{
  if (key == 'E')
  {
    extend_arrows = !extend_arrows;
  }

  if (key <'1' || key >'8')
    return false;

  viewer.data.clear();
  viewer.core.show_lines = false;
  viewer.core.show_texture = false;

  if (key == '1')
  {
    // Cross field
    viewer.data.set_mesh(V, F);
    viewer.data.add_edges(extend_arrows ? B - global_scale*X1 : B, B + global_scale*X1 ,Eigen::RowVector3d(1,0,0));
    viewer.data.add_edges(extend_arrows ? B - global_scale*X2 : B, B + global_scale*X2 ,Eigen::RowVector3d(0,0,1));
  }

  if (key == '2')
  {
    // Bisector field
    viewer.data.set_mesh(V, F);
    viewer.data.add_edges(extend_arrows ? B - global_scale*BIS1 : B, B + global_scale*BIS1 ,Eigen::RowVector3d(1,0,0));
    viewer.data.add_edges(extend_arrows ? B - global_scale*BIS2 : B, B + global_scale*BIS2 ,Eigen::RowVector3d(0,0,1));
  }

  if (key == '3')
  {
    // Bisector field combed
    viewer.data.set_mesh(V, F);
    viewer.data.add_edges(extend_arrows ? B - global_scale*BIS1_combed : B, B + global_scale*BIS1_combed ,Eigen::RowVector3d(1,0,0));
    viewer.data.add_edges(extend_arrows ? B - global_scale*BIS2_combed : B, B + global_scale*BIS2_combed ,Eigen::RowVector3d(0,0,1));
  }

  if (key == '4')
  {
    // Singularities and cuts
    viewer.data.set_mesh(V, F);

    // Plot cuts
    int l_count = Seams.sum();
    Eigen::MatrixXd P1(l_count,3);
    Eigen::MatrixXd P2(l_count,3);

    for (unsigned i=0; i<Seams.rows(); ++i)
    {
      for (unsigned j=0; j<Seams.cols(); ++j)
      {
        if (Seams(i,j) != 0)
        {
          P1.row(l_count-1) = V.row(F(i,j));
          P2.row(l_count-1) = V.row(F(i,(j+1)%3));
          l_count--;
        }
      }
    }

    viewer.data.add_edges(P1, P2, Eigen::RowVector3d(1, 0, 0));

    // Plot the singularities as colored dots (red for negative, blue for positive)
    for (unsigned i=0; i<singularityIndex.size();++i)
    {
      if (singularityIndex(i) < 2 && singularityIndex(i) > 0)
        viewer.data.add_points(V.row(i),Eigen::RowVector3d(1,0,0));
      else if (singularityIndex(i) > 2)
        viewer.data.add_points(V.row(i),Eigen::RowVector3d(0,1,0));
    }
  }

  if (key == '5')
  {
    // Singularities and cuts, original field
    // Singularities and cuts
    viewer.data.set_mesh(V, F);
    viewer.data.add_edges(extend_arrows ? B - global_scale*X1_combed : B, B + global_scale*X1_combed ,Eigen::RowVector3d(1,0,0));
    viewer.data.add_edges(extend_arrows ? B - global_scale*X2_combed : B, B + global_scale*X2_combed ,Eigen::RowVector3d(0,0,1));

    // Plot cuts
    int l_count = Seams.sum();
    Eigen::MatrixXd P1(l_count,3);
    Eigen::MatrixXd P2(l_count,3);

    for (unsigned i=0; i<Seams.rows(); ++i)
    {
      for (unsigned j=0; j<Seams.cols(); ++j)
      {
        if (Seams(i,j) != 0)
        {
          P1.row(l_count-1) = V.row(F(i,j));
          P2.row(l_count-1) = V.row(F(i,(j+1)%3));
          l_count--;
        }
      }
    }

    viewer.data.add_edges(P1, P2, Eigen::RowVector3d(1, 0, 0));

    // Plot the singularities as colored dots (red for negative, blue for positive)
    for (unsigned i=0; i<singularityIndex.size();++i)
    {
      if (singularityIndex(i) < 2 && singularityIndex(i) > 0)
        viewer.data.add_points(V.row(i),Eigen::RowVector3d(1,0,0));
      else if (singularityIndex(i) > 2)
        viewer.data.add_points(V.row(i),Eigen::RowVector3d(0,1,0));
    }
  }


  if (key == '6')
  {
    // Global parametrization UV
    viewer.data.set_mesh(UV, FUV);
    viewer.data.set_uv(UV);
    viewer.core.show_lines = true;
  }

  if (key == '7')
  {
    // Global parametrization in 3D
    viewer.data.set_mesh(V, F);
    viewer.data.set_uv(UV,FUV);
    viewer.core.show_texture = true;
  }

  if (key == '8')
  {
    // Global parametrization in 3D with seams
    viewer.data.set_mesh(V, F);
    viewer.data.set_uv(UV_seams,FUV_seams);
    viewer.core.show_texture = true;
  }

  //viewer.data.set_colors(Eigen::RowVector3d(1,1,1));

  // Replace the standard texture with an integer shift invariant texture
  Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic> texture_R, texture_G, texture_B;
  line_texture(texture_R, texture_G, texture_B);
  viewer.data.set_texture(texture_R, texture_B, texture_G);

  viewer.core.align_camera_center(viewer.data.V,viewer.data.F);

  return false;
}

int main(int argc, char *argv[])
{
  using namespace Eigen;

  if (argc != 2)
  {
    cout << "Usage ex1_bin mesh.obj" << endl;
    exit(0);
  }

  // Read mesh
  igl::readOFF(argv[1],V,F);

  // Compute face barycenters
  igl::barycenter(V, F, B);

  // Compute scale for visualizing fields
  global_scale =  .5*igl::avg_edge_length(V, F);


  // Contrain one face
  VectorXi b(1);
  b << 0;
  MatrixXd bc(1,3);
  bc << 1, 0, 0;

  // Create a smooth 4-RoSy field
  VectorXd S;
  igl::comiso::nrosy(V,F,b,bc,VectorXi(),VectorXd(),MatrixXd(),4,0.5,X1,S);

  // Find the the orthogonal vector
  MatrixXd B1,B2,B3;
  igl::local_basis(V,F,B1,B2,B3);
  X2 = igl::rotate_vectors(X1, VectorXd::Constant(1,M_PI/2), B1, B2);

  double gradient_size = 50;
  double iter = 0;
  double stiffness = 5.0;
  bool direct_round = 0;

  // Always work on the bisectors, it is more general
  igl::compute_frame_field_bisectors(V, F, X1, X2, BIS1, BIS2);

  // Comb the field, implicitly defining the seams
  igl::comb_cross_field(V, F, BIS1, BIS2, BIS1_combed, BIS2_combed);

  // Find the integer mismatches
  igl::cross_field_missmatch(V, F, BIS1_combed, BIS2_combed, true, MMatch);

  // Find the singularities
  igl::find_cross_field_singularities(V, F, MMatch, isSingularity, singularityIndex);

  // Cut the mesh, duplicating all vertices on the seams
  igl::cut_mesh_from_singularities(V, F, MMatch, Seams);

  // Comb the frame-field accordingly
  igl::comb_frame_field(V, F, X1, X2, BIS1_combed, BIS2_combed, X1_combed, X2_combed);

  // Global parametrization
  igl::comiso::miq(V,
           F,
           X1_combed,
           X2_combed,
           MMatch,
           isSingularity,
           Seams,
           UV,
           FUV,
           gradient_size,
           stiffness,
           direct_round,
           iter,
           5,
           true);

// Global parametrization (with seams, only for demonstration)
/*
igl::comiso::miq(V,
         F,
         X1_combed,
         X2_combed,
         MMatch,
         isSingularity,
         Seams,
         UV_seams,
         FUV_seams,
         gradient_size,
         stiffness,
         direct_round,
         iter,
         5,
         false);
*/
  // Plot the mesh
  igl::viewer::Viewer viewer;
  // Plot the original mesh with a texture parametrization
  key_down(viewer,'7',0);

  //vertex to face adjacency for MIQ V to UV conversion
  std::vector<std::vector<int> > VF, VFi;
  igl::vertex_triangle_adjacency(V,F,VF,VFi);

  //vertex to face adjacency for MIQ UV to V conversion
  std::vector<std::vector<int> > UVF, UVFi;
  igl::vertex_triangle_adjacency(UV, FUV, UVF, UVFi);

  // Add MIQ-Tools menu
  int vertexIndex = 0;
  bool addPoints = false;
  viewer.callback_init = [&](igl::viewer::Viewer& viewer)
  {
    viewer.ngui->addNewWindow(Eigen::Vector2i(200,10),"MIQ-Tools");
    viewer.ngui->addNewGroup("Find vertex");
    viewer.ngui->addVariable(vertexIndex,"Vertex Index", true);
    viewer.ngui->addVariable(addPoints, "add points");
    viewer.ngui->addButton("Find (UV)!", [&](){
        if(vertexIndex < 0 || vertexIndex > UV.rows()){
          std::cerr << "Vertex index " << vertexIndex << " not in range of UV.\n";
          return;
        }
        if(addPoints)
          viewer.data.add_points(UV.row(vertexIndex),Eigen::RowVector3d(1,1,0));
        else
          viewer.data.set_points(UV.row(vertexIndex),Eigen::RowVector3d(1,1,0));
    });
    viewer.ngui->addButton("Find (V)!", [&](){
        if(vertexIndex < 0 || vertexIndex > V.rows()){
          std::cerr << "Vertex index " << vertexIndex << " not in range of V.\n";
          return;
        }
        if(addPoints)
          viewer.data.add_points(V.row(vertexIndex),Eigen::RowVector3d(1,1,0));
        else
          viewer.data.set_points(V.row(vertexIndex),Eigen::RowVector3d(1,1,0));
    });
    viewer.ngui->addButton("Find converted UV->V", [&](){
        if(vertexIndex < 0 || vertexIndex > UV.rows()){
          std::cerr << "Vertex index " << vertexIndex << " not in range of UV.\n";
          return;
        }

        int vIndex = F(UVF[vertexIndex][0], UVFi[vertexIndex][0]);
        if(addPoints)
          viewer.data.add_points(V.row(vIndex), Eigen::RowVector3d(1,1,0));
        else
          viewer.data.set_points(V.row(vIndex), Eigen::RowVector3d(1,1,0));
        std::cout << "Index " << vertexIndex << "(UV) is index " << vIndex << "(V)\n";
    });
    viewer.ngui->addButton("Find converted V->UV", [&](){
        if(vertexIndex < 0 || vertexIndex > V.rows()){
          std::cerr << "Vertex index " << vertexIndex << " not in range of V.\n";
          return;
        }

        std::vector<int> uvIndices;
        for(int i = 0; i < VF[vertexIndex].size(); i++){
          uvIndices.push_back(FUV(VF[vertexIndex][i], VFi[vertexIndex][i]));
        }
        std::sort(uvIndices.begin(), uvIndices.end());
        for(auto it = uvIndices.begin(); it != uvIndices.end(); ++it){
          // Ignore duplicates
          if(it != uvIndices.begin())
            if(*it == *(it-1))
              continue;

          if(addPoints)
            viewer.data.add_points(UV.row(*it) ,Eigen::RowVector3d(1,1,0));
          else
            viewer.data.set_points(UV.row(*it) ,Eigen::RowVector3d(1,1,0));
        std::cout << "Index " << vertexIndex << "(V) is index " << *it << "(UV)\n";
        }
    });

    viewer.ngui->addNewGroup("Check results");
    viewer.ngui->addButton("Serialize results", [&](){
      auto path = nanogui::file_dialog({}, true);

      if(path != ""){
        MIQState miqstate;
        miqstate.UV = UV;
        miqstate.FUV = FUV;
        // Serialize the state of the application
        igl::serialize(miqstate,"MIQState",path,true);
        std::cout << "Serialized UV(" << UV.rows() << " rows) and FUV(" << FUV.rows() << " rows)\n";
      }
    });

    viewer.ngui->addButton("Check results with reference", [&](){
      auto path = nanogui::file_dialog({ {"*", "Any file"}, }, true);

      if(path != ""){
        MIQState miqstate_ref, miqstate;
        miqstate.UV = UV;
        miqstate.FUV = FUV;
        // Serialize the state of the application
        igl::deserialize(miqstate_ref,"MIQState",path);
        std::cout << "this = reference\n";
        miqstate_ref.printDiff(miqstate);
      }
    });

    viewer.ngui->layout();
    return false;
  };

  // Plot the original mesh with a texture parametrization
  key_down(viewer,'7',0);

  // Launch the viewer
  viewer.callback_key_down = &key_down;
  viewer.launch();
}
