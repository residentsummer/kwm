#include "node.h"
#include "container.h"
#include "tree.h"
#include "space.h"
#include "window.h"

#include "axlib/axlib.h"

extern std::map<CFStringRef, space_info> WindowTree;
extern ax_application *FocusedApplication;

extern kwm_settings KWMSettings;

tree_node *CreateRootNode()
{
    tree_node Clear = {0};
    tree_node *RootNode = (tree_node*) malloc(sizeof(tree_node));
    *RootNode = Clear;

    RootNode->WindowID = 0;
    RootNode->Type = NodeTypeTree;
    RootNode->Parent = NULL;
    RootNode->LeftChild = NULL;
    RootNode->RightChild = NULL;
    RootNode->SplitRatio = KWMSettings.SplitRatio;
    RootNode->SplitMode = SPLIT_OPTIMAL;

    return RootNode;
}

link_node *CreateLinkNode()
{
    link_node Clear = {0};
    link_node *Link = (link_node*) malloc(sizeof(link_node));
    *Link = Clear;

    Link->WindowID = 0;
    Link->Prev = NULL;
    Link->Next = NULL;

    return Link;
}

tree_node *CreateLeafNode(ax_display *Display, tree_node *Parent, int WindowID, int ContainerType)
{
    tree_node Clear = {0};
    tree_node *Leaf = (tree_node*) malloc(sizeof(tree_node));
    *Leaf = Clear;

    Leaf->Parent = Parent;
    Leaf->WindowID = WindowID;
    Leaf->Type = NodeTypeTree;

    CreateNodeContainer(Display, Leaf, ContainerType);

    Leaf->LeftChild = NULL;
    Leaf->RightChild = NULL;

    return Leaf;
}

void CreateLeafNodePair(ax_display *Display, tree_node *Parent, int FirstWindowID, int SecondWindowID, split_type SplitMode)
{
    Parent->WindowID = 0;
    Parent->SplitMode = SplitMode;
    Parent->SplitRatio = KWMSettings.SplitRatio;

    node_type ParentType = Parent->Type;
    link_node *ParentList = Parent->List;
    Parent->Type = NodeTypeTree;
    Parent->List = NULL;

    int LeftWindowID = KWMSettings.SpawnAsLeftChild ? SecondWindowID : FirstWindowID;
    int RightWindowID = KWMSettings.SpawnAsLeftChild ? FirstWindowID : SecondWindowID;

    if(SplitMode == SPLIT_VERTICAL)
    {
        Parent->LeftChild = CreateLeafNode(Display, Parent, LeftWindowID, 1);
        Parent->RightChild = CreateLeafNode(Display, Parent, RightWindowID, 2);

        tree_node *Node = KWMSettings.SpawnAsLeftChild ?  Parent->RightChild : Parent->LeftChild;
        Node->Type = ParentType;
        Node->List = ParentList;
        ResizeLinkNodeContainers(Node);
    }
    else if(SplitMode == SPLIT_HORIZONTAL)
    {
        Parent->LeftChild = CreateLeafNode(Display, Parent, LeftWindowID, 3);
        Parent->RightChild = CreateLeafNode(Display, Parent, RightWindowID, 4);

        tree_node *Node = KWMSettings.SpawnAsLeftChild ?  Parent->RightChild : Parent->LeftChild;
        Node->Type = ParentType;
        Node->List = ParentList;
        ResizeLinkNodeContainers(Node);
    }
    else
    {
        Parent->Parent = NULL;
        Parent->LeftChild = NULL;
        Parent->RightChild = NULL;
        Parent = NULL;
    }
}

void CreatePseudoNode()
{
    ax_display *Display = AXLibMainDisplay();
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(!FocusedApplication)
        return;

    ax_window *Window = FocusedApplication->Focus;
    if(!Window)
        return;

    tree_node *Node = GetTreeNodeFromWindowID(SpaceInfo->RootNode, Window->ID);
    if(Node)
    {
        split_type SplitMode = KWMSettings.SplitMode == SPLIT_OPTIMAL ? GetOptimalSplitMode(Node) : KWMSettings.SplitMode;
        CreateLeafNodePair(Display, Node, Node->WindowID, 0, SplitMode);
        ApplyTreeNodeContainer(Node);
    }
}

void RemovePseudoNode()
{
    ax_display *Display = AXLibMainDisplay();
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(!FocusedApplication)
        return;

    ax_window *Window = FocusedApplication->Focus;
    if(!Window)
        return;

    tree_node *Node = GetTreeNodeFromWindowID(SpaceInfo->RootNode, Window->ID);
    if(Node && Node->Parent)
    {
        tree_node *Parent = Node->Parent;
        tree_node *PseudoNode = IsLeftChild(Node) ? Parent->RightChild : Parent->LeftChild;
        if(!PseudoNode || !IsLeafNode(PseudoNode) || PseudoNode->WindowID != 0)
            return;

        Parent->WindowID = Node->WindowID;
        Parent->LeftChild = NULL;
        Parent->RightChild = NULL;
        free(Node);
        free(PseudoNode);
        ApplyTreeNodeContainer(Parent);
    }
}

bool IsLeafNode(tree_node *Node)
{
    return Node->LeftChild == NULL && Node->RightChild == NULL ? true : false;
}

bool IsPseudoNode(tree_node *Node)
{
    return Node && Node->WindowID == 0 && IsLeafNode(Node);
}

bool IsLeftChild(tree_node *Node)
{
    if(Node && IsLeafNode(Node))
    {
        tree_node *Parent = Node->Parent;
        return Parent->LeftChild == Node;
    }

    return false;
}

bool IsRightChild(tree_node *Node)
{
    if(Node && IsLeafNode(Node))
    {
        tree_node *Parent = Node->Parent;
        return Parent->RightChild == Node;
    }

    return false;
}

void ToggleFocusedNodeSplitMode()
{
    ax_display *Display = AXLibMainDisplay();
    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    if(!FocusedApplication)
        return;

    ax_window *Window = FocusedApplication->Focus;
    if(!Window)
        return;

    tree_node *Node = GetTreeNodeFromWindowID(SpaceInfo->RootNode, Window->ID);
    if(!Node)
        return;

    tree_node *Parent = Node->Parent;
    if(!Parent || IsLeafNode(Parent))
        return;

    Parent->SplitMode = Parent->SplitMode == SPLIT_VERTICAL ? SPLIT_HORIZONTAL : SPLIT_VERTICAL;
    CreateNodeContainers(Display, Parent, false);
    ApplyTreeNodeContainer(Parent);
}

void ToggleTypeOfFocusedNode()
{
    ax_window *Window = FocusedApplication ? FocusedApplication->Focus : NULL;
    if(!Window)
        return;

    ax_display *Display = AXLibWindowDisplay(Window);
    if(!Display)
        return;

    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    tree_node *TreeNode = GetTreeNodeFromWindowIDOrLinkNode(SpaceInfo->RootNode, Window->ID);
    if(TreeNode && TreeNode != SpaceInfo->RootNode)
        TreeNode->Type = TreeNode->Type == NodeTypeTree ? NodeTypeLink : NodeTypeTree;
}

void ChangeTypeOfFocusedNode(node_type Type)
{
    ax_window *Window = FocusedApplication ? FocusedApplication->Focus : NULL;
    if(!Window)
        return;

    ax_display *Display = AXLibWindowDisplay(Window);
    if(!Display)
        return;

    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    tree_node *TreeNode = GetTreeNodeFromWindowIDOrLinkNode(SpaceInfo->RootNode, Window->ID);
    if(TreeNode && TreeNode != SpaceInfo->RootNode)
        TreeNode->Type = Type;
}

void SwapNodeWindowIDs(tree_node *A, tree_node *B)
{
    if(A && B)
    {
        DEBUG("SwapNodeWindowIDs() " << A->WindowID << " with " << B->WindowID);
        int TempWindowID = A->WindowID;
        A->WindowID = B->WindowID;
        B->WindowID = TempWindowID;

        node_type TempNodeType = A->Type;
        A->Type = B->Type;
        B->Type = TempNodeType;

        link_node *TempLinkList = A->List;
        A->List = B->List;
        B->List = TempLinkList;

        ResizeLinkNodeContainers(A);
        ResizeLinkNodeContainers(B);
        ApplyTreeNodeContainer(A);
        ApplyTreeNodeContainer(B);
    }
}

void SwapNodeWindowIDs(link_node *A, link_node *B)
{
    if(A && B)
    {
        DEBUG("SwapNodeWindowIDs() " << A->WindowID << " with " << B->WindowID);
        int TempWindowID = A->WindowID;
        A->WindowID = B->WindowID;
        B->WindowID = TempWindowID;
        ResizeWindowToContainerSize(A);
        ResizeWindowToContainerSize(B);
    }
}

split_type GetOptimalSplitMode(tree_node *Node)
{
    return (Node->Container.Width / Node->Container.Height) >= KWMSettings.OptimalRatio ? SPLIT_VERTICAL : SPLIT_HORIZONTAL;
}

void ResizeWindowToContainerSize(tree_node *Node)
{
    ax_window *Window = GetWindowByID((unsigned int)Node->WindowID);
    if(Window)
    {
        SetWindowDimensions(Window, Node->Container.X, Node->Container.Y,
                            Node->Container.Width, Node->Container.Height);
    }
}

void ResizeWindowToContainerSize(link_node *Link)
{
    ax_window *Window = GetWindowByID((unsigned int)Link->WindowID);
    if(Window)
    {
        SetWindowDimensions(Window, Link->Container.X, Link->Container.Y,
                            Link->Container.Width, Link->Container.Height);
    }
}

void ResizeWindowToContainerSize(ax_window *Window)
{
    if(Window)
    {
        ax_window *Window = FocusedApplication ? FocusedApplication->Focus : NULL;
        if(!Window)
            return;

        ax_display *Display = AXLibWindowDisplay(Window);
        if(!Display)
            return;

        space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
        tree_node *Node = GetTreeNodeFromWindowID(SpaceInfo->RootNode, Window->ID);
        if(Node)
            ResizeWindowToContainerSize(Node);

        if(!Node)
        {
            link_node *Link = GetLinkNodeFromWindowID(SpaceInfo->RootNode, Window->ID);
            if(Link)
                ResizeWindowToContainerSize(Link);
        }
    }
}

void ResizeWindowToContainerSize()
{
    ax_window *Window = FocusedApplication ? FocusedApplication->Focus : NULL;
    if(!Window)
        return;

    ResizeWindowToContainerSize(Window);
}

void ModifyContainerSplitRatio(double Offset)
{
    ax_application *Application = AXLibGetFocusedApplication();
    if(!Application)
        return;

    ax_window *Window = Application->Focus;
    if(!Window)
        return;

    ax_display *Display = AXLibWindowDisplay(Window);
    if(!Display)
        return;

    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    tree_node *Root = SpaceInfo->RootNode;
    if(!Root || IsLeafNode(Root) || Root->WindowID != 0)
        return;

    tree_node *Node = GetTreeNodeFromWindowIDOrLinkNode(Root, Window->ID);
    if(Node && Node->Parent)
    {
        if(Node->Parent->SplitRatio + Offset > 0.0 &&
           Node->Parent->SplitRatio + Offset < 1.0)
        {
            Node->Parent->SplitRatio += Offset;
            ResizeNodeContainer(Display, Node->Parent);
            ApplyTreeNodeContainer(Node->Parent);
        }
    }
}

void ModifyContainerSplitRatio(double Offset, int Degrees)
{
    ax_application *Application = AXLibGetFocusedApplication();
    if(!Application)
        return;

    ax_window *Window = Application->Focus;
    if(!Window)
        return;

    ax_display *Display = AXLibWindowDisplay(Window);
    if(!Display)
        return;

    space_info *SpaceInfo = &WindowTree[Display->Space->Identifier];
    tree_node *Root = SpaceInfo->RootNode;
    if(!Root || IsLeafNode(Root) || Root->WindowID != 0)
        return;

    tree_node *Node = GetTreeNodeFromWindowIDOrLinkNode(Root, Window->ID);
    if(Node)
    {
        ax_window *ClosestWindow = NULL;
        if(FindClosestWindow(Degrees, &ClosestWindow, false))
        {
            tree_node *Target = GetTreeNodeFromWindowIDOrLinkNode(Root, ClosestWindow->ID);
            tree_node *Ancestor = FindLowestCommonAncestor(Node, Target);
            if(Ancestor &&
               Ancestor->SplitRatio + Offset > 0.0 &&
               Ancestor->SplitRatio + Offset < 1.0)
            {
                Ancestor->SplitRatio += Offset;
                ResizeNodeContainer(Display, Ancestor);
                ApplyTreeNodeContainer(Ancestor);
            }
        }
    }
}

tree_node *FindLowestCommonAncestor(tree_node *A, tree_node *B)
{
    if(!A || !B)
        return NULL;

    std::stack<tree_node*> PathToRootFromA;
    std::stack<tree_node*> PathToRootFromB;

    tree_node *PathA = A;
    while(PathA->Parent)
    {
        PathToRootFromA.push(PathA->Parent);
        PathA = PathA->Parent;
    }

    tree_node *PathB = B;
    while(PathB->Parent)
    {
        PathToRootFromB.push(PathB->Parent);
        PathB = PathB->Parent;
    }

    tree_node *LCA = NULL;
    while(!PathToRootFromA.empty() && !PathToRootFromB.empty())
    {
        tree_node *RootA = PathToRootFromA.top();
        PathToRootFromA.pop();

        tree_node *RootB = PathToRootFromB.top();
        PathToRootFromB.pop();

        if(RootA == RootB)
            LCA = RootA;
        else
            break;
    }

    return LCA;
}
