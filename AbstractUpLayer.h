#ifndef ABSTRACTUPLAYER_H_
#define ABSTRACTUPLAYER_H_

class AbstractUpLayer {
public:
    AbstractUpLayer() {}
    virtual ~AbstractUpLayer() {}
    virtual void receiveString(char* s) = 0;
};

#endif /* ABSTRACTUPLAYER_H_ */
