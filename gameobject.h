#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <QObject>
#include "transform.h"
#include "structures.h"


class GameObject : public QObject
{
    Q_OBJECT
private:
    template<typename FuncType, typename ...Args>
    using QListType = QList<decltype(std::declval<FuncType>()(std::declval<Args>()...))>;
public:

    explicit GameObject(QObject *parent = nullptr);

    // 相对于自身的变换
    void localTranslate(const Vec3 &v);
    void moveToLocalPos(const Vec3 &v);

    // 相对于父对象的变换
    void translate(const Vec3 &v);
    void rotateAroundAxis(const Vec3 &axis, float rad);

    void setTransform(const Transform &t);
    const Transform &getTransform()const;
    Transform getGlobalTransform() const;
    virtual void updatePosition(const Transform &t);
    virtual void submitForRender();

    GameObject *parent() const;
    QList<GameObject*> children() const;

    friend class Stage3D;

    template<typename ObjectType, typename FuncType, typename ...Args> requires
        (requires(FuncType &&func, ObjectType *rt, Args ...args){
            {func(rt, std::forward<Args>(args)...)} -> std::semiregular;
        }
        || requires(FuncType &&func, ObjectType *rt, Args ...args){
            {func(rt, std::forward<Args>(args)...)} -> std::same_as<void>;
        }) && std::derived_from<ObjectType, GameObject>

        auto forEach(FuncType &&func, Args&& ...args)
            ->std::conditional_t<std::same_as<decltype(func(std::declval<ObjectType*>(), std::forward<Args>(args)...)), void>, void,
                        QListType<FuncType, ObjectType*, Args...>>
    {
        return m_forEachObject<ObjectType>(std::forward<FuncType>(func), this, std::forward<Args>(args)...);
    }

    template<typename ObjectType, typename FuncType, typename ...Args> requires
        (requires(FuncType &&func, ObjectType *rt, Args ...args){
            {func(rt, std::forward<Args>(args)...)} -> std::semiregular;
        }
        || requires(FuncType &&func, ObjectType *rt, Args ...args){
            {func(rt, std::forward<Args>(args)...)} -> std::same_as<void>;
        }) && std::derived_from<ObjectType, GameObject>

        auto forEach(FuncType &&func, Args&& ...args) const
        ->std::conditional_t<std::same_as<decltype(func(std::declval<ObjectType*>(), std::forward<Args>(args)...)), void>, void,
                            QListType<FuncType, const ObjectType*, Args...>>
    {
        return m_forEachObject<ObjectType>(std::forward<FuncType>(func), this, std::forward<Args>(args)...);
    }


private:
    bool modified;
    Transform transform;

    template<typename ObjectType, typename SelfType, typename FuncType, typename ...Args> requires
        requires(FuncType &&func, ObjectType *rt, Args ...args){
            {func(rt, std::forward<Args>(args)...)} -> std::semiregular;
        } && std::derived_from<ObjectType, GameObject>
    auto m_forEachObject(FuncType &&func, SelfType *self, Args&& ...args)
    -> QListType<FuncType, ObjectType*, Args...>{
        QListType<FuncType, ObjectType*, Args...> curr;
        ObjectType *casted = dynamic_cast<ObjectType*>(self);
        if(casted != nullptr)
            curr.push_back(func(casted, std::forward<Args>(args)...));
        for(GameObject *child:self->children()){
            curr.append(m_forEachObject<ObjectType>(std::forward<FuncType>(func), child, std::forward<Args>(args)...));
        }
        return curr;
    }
    template<typename ObjectType, typename SelfType, typename FuncType, typename ...Args> requires
        requires(FuncType &&func, ObjectType *rt, Args ...args){
            {func(rt, std::forward<Args>(args)...)} -> std::same_as<void>;
        } && std::derived_from<ObjectType, GameObject>
    void m_forEachObject(FuncType &&func, SelfType *self, Args&& ...args){
        ObjectType *casted = dynamic_cast<ObjectType*>(self);
        if(casted != nullptr)
            func(self, std::forward<Args>(args)...);
        for(GameObject *child:self->children()){
            m_forEachObject<ObjectType>(std::forward<FuncType>(func), child, std::forward<Args>(args)...);
        }
    }


signals:
};

class MeshActor : public GameObject{
    Q_OBJECT
public:
    uint meshID;
    Mesh mesh;
    explicit MeshActor(uint _meshID, QObject *parent = nullptr);
    void updatePosition(const Transform &t) override;
    void submitForRender() override;
};

class Camera : public GameObject{
    Q_OBJECT
public:
    CameraInfo camInfo;
    explicit Camera(const CameraInfo &info, QObject *parent = nullptr);
    void updatePosition(const Transform &t) override;
    Ray pixelToRay(int w, int h)const;
};


#endif // GAMEOBJECT_H
