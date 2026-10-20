#include <unity.h>

#include "../../src/pet/pet_sim.cpp"

static const uint32_t T0 = 1760000000;  // arbitrary wall-clock start
static int fixed_hour = 12;
static int hour_fixed(uint32_t) { return fixed_hour; }

static PetState hatched_pet() {
  PetState s;
  pet_init(s, 42, T0);
  pet_sim_advance(s, T0 + 300, hour_fixed);
  return s;
}

void setUp() { fixed_hour = 12; }
void tearDown() {}

void test_egg_hatches_after_five_minutes() {
  PetState s;
  pet_init(s, 42, T0);
  TEST_ASSERT_EQUAL(0, pet_sim_advance(s, T0 + 240, hour_fixed) & EVT_HATCHED);
  TEST_ASSERT_EQUAL(STAGE_EGG, s.stage);
  TEST_ASSERT_TRUE(pet_sim_advance(s, T0 + 300, hour_fixed) & EVT_HATCHED);
  TEST_ASSERT_EQUAL(STAGE_BABY, s.stage);
  TEST_ASSERT_EQUAL(FORM_BLOB, s.form);
}

void test_egg_does_not_decay() {
  PetState s;
  pet_init(s, 42, T0);
  pet_sim_advance(s, T0 + 240, hour_fixed);
  TEST_ASSERT_EQUAL_FLOAT(80, s.hunger);
}

void test_hunger_decays_at_baby_rate() {
  PetState s = hatched_pet();
  float before = s.hunger;
  pet_sim_advance(s, s.last_epoch + 1800, hour_fixed);  // half an hour
  TEST_ASSERT_FLOAT_WITHIN(0.1f, before - 6.0f, s.hunger);
}

void test_sleep_slows_hunger() {
  PetState awake = hatched_pet();
  PetState asleep = hatched_pet();
  fixed_hour = 23;
  pet_sim_advance(asleep, asleep.last_epoch + 1800, hour_fixed);
  fixed_hour = 12;
  pet_sim_advance(awake, awake.last_epoch + 1800, hour_fixed);
  TEST_ASSERT_TRUE(asleep.asleep);
  TEST_ASSERT_FALSE(awake.asleep);
  TEST_ASSERT_TRUE(asleep.hunger > awake.hunger);
}

void test_catchup_matches_incremental_advance() {
  PetState bulk = hatched_pet();
  PetState inc = hatched_pet();
  uint32_t target = bulk.last_epoch + 36 * 3600;
  pet_sim_advance(bulk, target, hour_fixed);
  for (uint32_t t = inc.last_epoch + 7; t <= target; t += 7) pet_sim_advance(inc, t, hour_fixed);
  pet_sim_advance(inc, target, hour_fixed);
  TEST_ASSERT_EQUAL_MEMORY(&bulk, &inc, sizeof(PetState));
}

void test_catchup_capped_at_seven_days() {
  PetState s = hatched_pet();
  uint32_t start = s.last_epoch;
  pet_sim_advance(s, start + 30 * 24 * 3600, hour_fixed);
  TEST_ASSERT_EQUAL_UINT32(start + 30 * 24 * 3600, s.last_epoch);
}

void test_good_care_evolves_to_bun() {
  PetState s = hatched_pet();
  uint32_t ev = pet_sim_advance(s, s.last_epoch + 3600, hour_fixed);
  TEST_ASSERT_TRUE(ev & EVT_EVOLVED);
  TEST_ASSERT_EQUAL(STAGE_CHILD, s.stage);
  TEST_ASSERT_EQUAL(FORM_BUN, s.form);
}

void test_poor_care_evolves_to_sprout() {
  PetState s = hatched_pet();
  s.stage_mistakes = 3;
  pet_sim_advance(s, s.last_epoch + 3600, hour_fixed);
  TEST_ASSERT_EQUAL(FORM_SPROUT, s.form);
}

void test_light_left_on_at_night_is_a_care_mistake() {
  PetState s = hatched_pet();
  fixed_hour = 23;
  uint32_t ev = pet_sim_advance(s, s.last_epoch + 3600, hour_fixed);
  TEST_ASSERT_TRUE(ev & EVT_CARE_MISTAKE);
  TEST_ASSERT_EQUAL(1, s.care_mistakes);

  PetState dark = hatched_pet();
  dark.light_on = false;
  pet_sim_advance(dark, dark.last_epoch + 3600, hour_fixed);
  TEST_ASSERT_EQUAL(0, dark.care_mistakes);
}

void test_uncleaned_poop_causes_sickness_and_medicine_cures() {
  PetState s = hatched_pet();
  s.poop = 2;
  uint32_t ev = pet_sim_advance(s, s.last_epoch + 2 * 3600, hour_fixed);
  TEST_ASSERT_TRUE(ev & EVT_GOT_SICK);
  TEST_ASSERT_TRUE(s.sick);
  TEST_ASSERT_TRUE(pet_medicine(s));
  TEST_ASSERT_FALSE(s.sick);
}

void test_overfeeding_causes_sickness() {
  PetState s = hatched_pet();
  s.coins = 100;
  s.hunger = 100;
  for (int i = 0; i < 3; i++) TEST_ASSERT_TRUE(pet_feed(s, FOOD_SNACK));
  TEST_ASSERT_TRUE(s.sick);
}

void test_overfeed_count_resets_once_hungry_again() {
  PetState s = hatched_pet();
  s.coins = 100;
  s.hunger = 100;
  TEST_ASSERT_TRUE(pet_feed(s, FOOD_SNACK));
  TEST_ASSERT_TRUE(pet_feed(s, FOOD_SNACK));
  s.hunger = 40;
  pet_sim_advance(s, s.last_epoch + 60, hour_fixed);
  s.hunger = 100;
  TEST_ASSERT_TRUE(pet_feed(s, FOOD_SNACK));
  TEST_ASSERT_FALSE(s.sick);
}

void test_light_comes_back_on_in_daytime_even_if_woken_early() {
  PetState s = hatched_pet();
  fixed_hour = 23;
  pet_sim_advance(s, s.last_epoch + 3600, hour_fixed);  // light-on mistake tonight
  TEST_ASSERT_TRUE(s.light_mistake_tonight);
  pet_wake(s, s.last_epoch);
  s.light_on = false;
  fixed_hour = 9;
  pet_sim_advance(s, s.last_epoch + 60, hour_fixed);
  TEST_ASSERT_TRUE(s.light_on);
  TEST_ASSERT_FALSE(s.light_mistake_tonight);
}

void test_feed_requires_coins_and_awake() {
  PetState s = hatched_pet();
  s.coins = 4;
  TEST_ASSERT_FALSE(pet_feed(s, FOOD_MEAL));
  s.coins = 50;
  s.asleep = true;
  TEST_ASSERT_FALSE(pet_feed(s, FOOD_MEAL));
  s.asleep = false;
  TEST_ASSERT_TRUE(pet_feed(s, FOOD_MEAL));
  TEST_ASSERT_EQUAL_UINT32(45, s.coins);
}

void test_neglect_makes_pet_leave_and_new_egg_appears() {
  PetState s = hatched_pet();
  s.coins = 77;
  uint32_t ev = 0;
  while (!(ev & EVT_LEFT) && s.age_s < 7 * 24 * 3600) ev = pet_sim_advance(s, s.last_epoch + 60, hour_fixed);
  TEST_ASSERT_TRUE(ev & EVT_LEFT);
  TEST_ASSERT_EQUAL(1, s.generation);
  TEST_ASSERT_EQUAL(STAGE_EGG, s.stage);
  TEST_ASSERT_EQUAL_UINT32(77, s.coins);
}

void test_blob_roundtrip_and_rejection() {
  PetState s = hatched_pet();
  PetBlob blob;
  pet_blob_pack(s, blob);
  PetState out;
  TEST_ASSERT_TRUE(pet_blob_unpack(blob, out));
  TEST_ASSERT_EQUAL_MEMORY(&s, &out, sizeof(PetState));

  PetBlob corrupt = blob;
  corrupt.state.coins ^= 1;
  TEST_ASSERT_FALSE(pet_blob_unpack(corrupt, out));

  PetBlob old = blob;
  old.version = PET_STATE_VERSION + 1;
  TEST_ASSERT_FALSE(pet_blob_unpack(old, out));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_egg_hatches_after_five_minutes);
  RUN_TEST(test_egg_does_not_decay);
  RUN_TEST(test_hunger_decays_at_baby_rate);
  RUN_TEST(test_sleep_slows_hunger);
  RUN_TEST(test_catchup_matches_incremental_advance);
  RUN_TEST(test_catchup_capped_at_seven_days);
  RUN_TEST(test_good_care_evolves_to_bun);
  RUN_TEST(test_poor_care_evolves_to_sprout);
  RUN_TEST(test_light_left_on_at_night_is_a_care_mistake);
  RUN_TEST(test_uncleaned_poop_causes_sickness_and_medicine_cures);
  RUN_TEST(test_overfeeding_causes_sickness);
  RUN_TEST(test_overfeed_count_resets_once_hungry_again);
  RUN_TEST(test_light_comes_back_on_in_daytime_even_if_woken_early);
  RUN_TEST(test_feed_requires_coins_and_awake);
  RUN_TEST(test_neglect_makes_pet_leave_and_new_egg_appears);
  RUN_TEST(test_blob_roundtrip_and_rejection);
  return UNITY_END();
}
