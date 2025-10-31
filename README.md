# Monopoly Game Logic Documentation

## Overview
This is a simplified two-player Monopoly game implementation built with SDL2. The game follows classic Monopoly rules with property purchasing, rent collection, mortgage system, and property upgrades.

---

## Game States

The game operates through four distinct states:

1. **Game_STATE_BEGIN_MOVE**: Player's turn to roll dice and move
2. **Game_STATE_BUY_PROPERTY**: Player landed on unowned property and can purchase it
3. **Game_STATE_PLAYER_IN_DEBT**: Player has negative balance and must sell properties or declare bankruptcy
4. **Game_STATE_END**: Game over, a winner has been determined

---

## Players

### Player Configuration
- **Total Players**: 2
- **Player 1**: Red
- **Player 2**: Blue
- **Starting Money**: $1500 each
- **Starting Position**: 0 (GO)

### Player Data Structure
```c
typedef struct {
    int id;           // Player identifier (0 or 1)
    char* name;       // "Red" or "Blue"
    int money;        // Current money balance
    int jailed;       // Jail status (not fully implemented)
    int position;     // Current board position (0-39)
} Game_Player;
```

---

## Board Layout

### Board Positions (40 total)
The board follows the classic Monopoly layout:

| Position | Type | Details |
|----------|------|---------|
| 0 | GO | Starting position, collect $200 when passing |
| 1, 3 | Purple | Price: $60 |
| 2, 17, 33 | Community Chest | Not implemented |
| 4 | Income Tax | Pay $200 |
| 5, 15, 25, 35 | Railroad | Price: $200 |
| 6, 8, 9 | Light Blue | Price: $100 (9 is $120) |
| 7, 22, 36 | Chance | Not implemented |
| 10 | Jail | Just visiting |
| 11, 13, 14 | Magenta | Price: $140-$160 |
| 12, 28 | Utility | Price: $150 |
| 16, 18, 19 | Orange | Price: $180-$200 |
| 20 | Free Parking | No action |
| 21, 23, 24 | Red | Price: $220-$240 |
| 26, 27, 29 | Yellow | Price: $260-$280 |
| 30 | Go To Jail | Not implemented |
| 31, 32, 34 | Green | Price: $300-$320 |
| 37, 39 | Blue | Price: $350-$400 |
| 38 | Luxury Tax | Pay $75 |

---

## Property System

### Property Data Structure
```c
typedef struct {
    Game_Prop_Type type;  // Property color/type
    int id;               // Position on board (0-39)
    int owner;            // Player ID (-1 if unowned)
    int mortaged;         // 1 if mortgaged, 0 otherwise
    int upgrades;         // 0-5 (0=no houses, 1-4=houses, 5=hotel)
    int price;            // Purchase price
    int upgradeCost;      // Cost to build house
    int rentCost[6];      // Rent at each upgrade level
} Game_Prop;
```

### Property Types
- **Colored Properties**: Purple, Light Blue, Magenta, Orange, Red, Yellow, Green, Blue
- **Railroads**: 4 total (positions 5, 15, 25, 35)
- **Utilities**: 2 total (positions 12, 28)
- **Special Squares**: GO, Taxes, Jail, Free Parking, Chance, Community Chest

---

## Core Game Mechanics

### 1. Turn Flow

1. **Roll Dice**
   - Two six-sided dice (1-6 each)
   - Player moves forward by sum of dice
   - If doubles are rolled, player gets another turn
   - Passing GO awards $200

2. **Land on Property**
   - **Unowned**: Enter BUY_PROPERTY state
   - **Owned by opponent**: Pay rent
   - **Owned by self**: No action
   - **Special squares**: Apply special rules

3. **End Turn**
   - If no doubles rolled, next player's turn
   - If doubles rolled, same player rolls again

### 2. Buying Properties

**Conditions**:
- Property must be unowned (owner == -1)
- Player must have enough money
- Player must be on the property

**Process**:
- Press SPACE when in BUY_PROPERTY state
- Money is deducted from player
- Property owner is set to current player

### 3. Rent System

#### Standard Properties
- **Base rent**: `rentCost[0]` when unupgraded
- **Monopoly bonus**: 2x rent if player owns all properties of that color (and no houses)
- **With houses**: `rentCost[1]` through `rentCost[4]` (1-4 houses)
- **With hotel**: `rentCost[5]` (5th upgrade level)

#### Railroads
- **Formula**: `$25 × 2^(number_owned-1)`
- 1 railroad: $25
- 2 railroads: $50
- 3 railroads: $100
- 4 railroads: $200
- Only counts unmortgaged railroads

#### Utilities
- **1 utility owned**: 4 × dice roll
- **2 utilities owned**: 10 × dice roll
- Only counts unmortgaged utilities

### 4. Property Upgrades (Houses/Hotels)

#### Building Requirements (Press B)
- Must own monopoly (all properties of same color)
- None of the properties in the color group can be mortgaged
- Must build evenly across color group (no property can be more than 1 upgrade ahead)
- Must have enough money for upgrade cost
- Maximum 5 upgrades per property (4 houses, then 1 hotel)

#### Selling Houses (Press D)
- Must sell evenly (property must have highest upgrade level in color group)
- Receive half of upgrade cost back
- Can sell to raise money when in debt

### 5. Mortgage System (Press M)

#### Mortgaging a Property
- **Requirements**:
  - Must own the property
  - Property cannot have houses (must sell all houses first)
  - No other property in the color group can have houses
- **Benefit**: Receive 50% of property price
- **Effect**: Property shows as mortgaged, no rent collected

#### Unmortgaging a Property
- **Cost**: 55% of property price (110% of mortgage value)
- **Effect**: Property becomes active again, can collect rent

### 6. Debt Management

**When player goes into debt (money < 0)**:
- Game enters PLAYER_IN_DEBT state
- Player must:
  - Sell houses (Press D)
  - Mortgage properties (Press M)
  - Or declare bankruptcy (Press X)

**Bankruptcy**:
- Press X to declare bankruptcy
- Current player loses
- Other player wins
- Game enters END state

---

## Special Squares

### GO (Position 0)
- **Landing**: No special action
- **Passing**: Collect $200 automatically

### Income Tax (Position 4)
- Pay $200 when landing

### Luxury Tax (Position 38)
- Pay $75 when landing

### Jail (Position 10)
- Just visiting (no penalty in this implementation)

### Free Parking (Position 20)
- No action

### Chance & Community Chest
- Positions: 2, 7, 17, 22, 33, 36
- **Not implemented** in current version

### Go To Jail (Position 30)
- **Not fully implemented** in current version

---

## User Controls

### Keyboard Controls
- **SPACE**: 
  - Roll dice and move (BEGIN_MOVE state)
  - Buy property (BUY_PROPERTY state)
  - Continue after debt resolution (PLAYER_IN_DEBT state)

- **M**: Mortgage/Unmortgage selected property

- **B**: Build house on selected property

- **D**: Destroy (sell) house on selected property

- **X**: Declare bankruptcy

### Mouse Controls
- **Left Click**: Select property on the board
  - Cross marker appears on selected property
  - Property must be clicked to perform M/B/D actions

---

## Win/Loss Conditions

### Winning
- Opponent declares bankruptcy
- Opponent has negative money and cannot/will not sell assets

### Game End
- One player declares bankruptcy (Press X)
- Game displays: "[Loser] lost! [Winner] won!"
- Game enters END state (no further actions possible)

---

## Monopoly Rules (Color Groups)

To have a monopoly and build houses, players must own:

- **Purple**: 2 properties (1, 3)
- **Light Blue**: 3 properties (6, 8, 9)
- **Magenta**: 3 properties (11, 13, 14)
- **Orange**: 3 properties (16, 18, 19)
- **Red**: 3 properties (21, 23, 24)
- **Yellow**: 3 properties (26, 27, 29)
- **Green**: 3 properties (31, 32, 34)
- **Blue**: 2 properties (37, 39)

**Note**: Railroads and Utilities cannot have houses built on them.

---

## Property Prices and Rents

### Purple Properties
- **Price**: $60
- **Upgrade Cost**: $50
- **Rents**: $2, $10, $30, $90, $160, $250

### Light Blue Properties
- **Price**: $100
- **Upgrade Cost**: $50
- **Rents**: $6-$8, $30-$40, $90-$100, $270-$300, $400-$450, $550-$600

### Magenta Properties
- **Price**: $140-$160
- **Upgrade Cost**: $100
- **Rents**: $10-$12, $50-$60, $150-$180, $450-$500, $625-$700, $750-$900

### Orange Properties
- **Price**: $180-$200
- **Upgrade Cost**: $100
- **Rents**: $14-$16, $70-$80, $200-$220, $550-$600, $750-$800, $950-$1000

### Red Properties
- **Price**: $220-$240
- **Upgrade Cost**: $150
- **Rents**: $18-$20, $90-$100, $250-$300, $700-$750, $875-$925, $1050-$1100

### Yellow Properties
- **Price**: $260-$280
- **Upgrade Cost**: $150
- **Rents**: $22-$24, $110-$120, $330-$360, $800-$850, $975-$1025, $1150-$1200

### Green Properties
- **Price**: $300-$320
- **Upgrade Cost**: $200
- **Rents**: $26-$28, $130-$150, $390-$450, $900-$1000, $1100-$1200, $1275-$1400

### Blue Properties
- **Price**: $350-$400
- **Upgrade Cost**: $200
- **Rents**: $35-$50, $175-$200, $500-$600, $1100-$1400, $1300-$1700, $1500-$2000

---

## Game Strategy Tips

1. **Early Game**: Focus on completing color groups for monopolies
2. **Railroads**: Very profitable if you own multiple (good steady income)
3. **Utilities**: Less valuable than railroads, income depends on dice rolls
4. **Building**: Orange and Red properties offer best return on investment
5. **Cash Management**: Always keep emergency cash for rent
6. **Mortgaging**: Use strategically when in debt, but remember 10% unmortgage fee
7. **Even Building**: Build evenly across your monopolies for maximum benefit

---

## Implementation Notes

### Features Not Fully Implemented
- Jail mechanics (go to jail, get out of jail)
- Chance cards
- Community Chest cards
- Trading between players
- More than 2 players
- House/hotel piece limits

### Known Bugs
- GO square switch statement has unreachable code (lines 380-381)
- No validation for house limits (official rules: 32 houses, 12 hotels)

---

## Technical Details

### Random Number Generation
- Uses C standard `rand()` with `time(NULL)` seed
- Dice rolls: `rand() % 6 + 1` for each die

### Memory Management
- Dynamic allocation for messages and status strings
- Strings must be freed after use to prevent memory leaks

### State Machine
The game uses a state machine pattern where each state determines available actions and valid transitions.

---

## Code Architecture

### Main Files
- **Game.c**: Core game logic, state management, property management
- **Game.h**: Public API for game functions
- **Player.h**: Player data structure
- **Property.h**: Property data structure and types
- **Render.c**: SDL2 rendering, UI, event handling

### Key Functions
- `Game_init()`: Initialize game, players, and properties
- `Game_cycle()`: Execute one turn (roll, move, land)
- `Game_buyProperty()`: Purchase current property
- `Game_receiveinput()`: Process keyboard input
- `Game_mortageProp()`: Mortgage/unmortgage property
- `Game_upgradeProp()`: Build/destroy houses
- `Game_goBankrupt()`: End game with bankruptcy

